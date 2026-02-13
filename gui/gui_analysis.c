/*
 * gui_analysis.c — Background analysis pipeline for SpeciesID GUI.
 *
 * Mirrors the logic in src/main.c cmd_run(), but runs on an SDL thread
 * and reports incremental state/progress to the GUI.
 *
 * Supports per-sample analysis with R1/R2 pair detection.
 */
#include "gui_analysis.h"
#include "utils.h"
#include "refdb.h"
#include "index.h"
#include "fastq.h"
#include "classify.h"
#include "em.h"
#include "report.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* State labels                                                        */
/* ------------------------------------------------------------------ */
static const char *state_labels[] = {
    [ANALYSIS_IDLE]              = "Ready",
    [ANALYSIS_LOADING_INDEX]     = "Loading index...",
    [ANALYSIS_READING_FASTQ]     = "Reading FASTQ...",
    [ANALYSIS_CLASSIFYING]       = "Classifying reads...",
    [ANALYSIS_RUNNING_EM]        = "Running EM...",
    [ANALYSIS_GENERATING_REPORT] = "Generating report...",
    [ANALYSIS_DONE]              = "Done",
    [ANALYSIS_ERROR]             = "Error",
};

const char *analysis_state_label(analysis_state_t s) {
    if (s < 0 || s > ANALYSIS_ERROR) return "Unknown";
    return state_labels[s];
}

/* ------------------------------------------------------------------ */
/* Sample detection helpers                                            */
/* ------------------------------------------------------------------ */

/* Extract basename from a path (pointer into the original string). */
static const char *path_basename(const char *path) {
    const char *s = strrchr(path, '/');
    if (!s) s = strrchr(path, '\\');
    return s ? s + 1 : path;
}

/* Strip known sequence file extensions from basename into buf.
   Extensions: .fq.gz, .fastq.gz, .fa.gz, .fasta.gz, .fq, .fastq, .fa, .fasta */
static void strip_seq_extension(const char *basename, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s", basename);
    size_t len = strlen(buf);

    /* Try double extensions first (.fq.gz etc.) */
    static const char *dbl_exts[] = {
        ".fq.gz", ".fastq.gz", ".fa.gz", ".fasta.gz", NULL
    };
    for (const char **e = dbl_exts; *e; e++) {
        size_t elen = strlen(*e);
        if (len > elen) {
            int match = 1;
            for (size_t i = 0; i < elen; i++) {
                if (tolower((unsigned char)buf[len - elen + i]) !=
                    tolower((unsigned char)(*e)[i])) {
                    match = 0;
                    break;
                }
            }
            if (match) { buf[len - elen] = '\0'; return; }
        }
    }

    /* Single extensions */
    static const char *sgl_exts[] = {
        ".fastq", ".fasta", ".fq", ".fa", ".gz", NULL
    };
    for (const char **e = sgl_exts; *e; e++) {
        size_t elen = strlen(*e);
        if (len > elen) {
            int match = 1;
            for (size_t i = 0; i < elen; i++) {
                if (tolower((unsigned char)buf[len - elen + i]) !=
                    tolower((unsigned char)(*e)[i])) {
                    match = 0;
                    break;
                }
            }
            if (match) { buf[len - elen] = '\0'; return; }
        }
    }
}

/* Try to strip R1/R2 suffix from a name.
   Recognised patterns: _R1, _R2, _1, _2 (case insensitive, at end).
   Returns: 1 if R1 found, 2 if R2 found, 0 if neither.
   core_out receives the name with the suffix removed. */
static int strip_read_suffix(const char *name, char *core_out, size_t core_sz) {
    snprintf(core_out, core_sz, "%s", name);
    size_t len = strlen(core_out);

    /* _R1 / _R2 */
    if (len >= 3) {
        char c1 = (char)tolower((unsigned char)core_out[len - 3]);
        char c2 = (char)tolower((unsigned char)core_out[len - 2]);
        char c3 = core_out[len - 1];
        if (c1 == '_' && c2 == 'r' && (c3 == '1' || c3 == '2')) {
            int which = (c3 == '1') ? 1 : 2;
            core_out[len - 3] = '\0';
            return which;
        }
    }
    /* _1 / _2 */
    if (len >= 2) {
        char c1 = core_out[len - 2];
        char c2 = core_out[len - 1];
        if (c1 == '_' && (c2 == '1' || c2 == '2')) {
            int which = (c2 == '1') ? 1 : 2;
            core_out[len - 2] = '\0';
            return which;
        }
    }
    return 0;
}

/* Compare two strings case-insensitively. */
static int strcasecmp_portable(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

void detect_samples(analysis_context_t *ctx) {
    ctx->n_samples = 0;
    if (ctx->n_fastq_files == 0) return;

    /* Per-file metadata */
    char stems[32][256];      /* basename with extension stripped */
    char cores[32][256];      /* stem with R1/R2 suffix stripped  */
    int  read_num[32];        /* 0=none, 1=R1, 2=R2              */
    int  used[32];
    memset(used, 0, sizeof(used));

    for (int i = 0; i < ctx->n_fastq_files; i++) {
        const char *bn = path_basename(ctx->fastq_paths[i]);
        strip_seq_extension(bn, stems[i], sizeof(stems[i]));
        read_num[i] = strip_read_suffix(stems[i], cores[i], sizeof(cores[i]));
    }

    /* Pair R1 with R2 */
    for (int i = 0; i < ctx->n_fastq_files && ctx->n_samples < 32; i++) {
        if (used[i]) continue;
        if (read_num[i] == 1) {
            /* Look for matching R2 */
            int mate = -1;
            for (int j = i + 1; j < ctx->n_fastq_files; j++) {
                if (used[j]) continue;
                if (read_num[j] == 2 &&
                    strcasecmp_portable(cores[i], cores[j]) == 0) {
                    mate = j;
                    break;
                }
            }
            if (mate >= 0) {
                sample_group_t *sg = &ctx->samples[ctx->n_samples++];
                snprintf(sg->sample_name, sizeof(sg->sample_name),
                         "%s", cores[i]);
                sg->file_indices[0] = i;
                sg->file_indices[1] = mate;
                sg->n_files = 2;
                used[i] = 1;
                used[mate] = 1;
                continue;
            }
        }
        if (read_num[i] == 2) {
            /* Look for matching R1 */
            int mate = -1;
            for (int j = i + 1; j < ctx->n_fastq_files; j++) {
                if (used[j]) continue;
                if (read_num[j] == 1 &&
                    strcasecmp_portable(cores[i], cores[j]) == 0) {
                    mate = j;
                    break;
                }
            }
            if (mate >= 0) {
                sample_group_t *sg = &ctx->samples[ctx->n_samples++];
                snprintf(sg->sample_name, sizeof(sg->sample_name),
                         "%s", cores[i]);
                sg->file_indices[0] = mate;  /* R1 first */
                sg->file_indices[1] = i;     /* R2 second */
                sg->n_files = 2;
                used[i] = 1;
                used[mate] = 1;
                continue;
            }
        }
    }

    /* Remaining unpaired files become single-file samples */
    for (int i = 0; i < ctx->n_fastq_files && ctx->n_samples < 32; i++) {
        if (used[i]) continue;
        sample_group_t *sg = &ctx->samples[ctx->n_samples++];
        snprintf(sg->sample_name, sizeof(sg->sample_name), "%s", stems[i]);
        sg->file_indices[0] = i;
        sg->n_files = 1;
        used[i] = 1;
    }

    /* Sort samples alphabetically by name (simple insertion sort) */
    for (int i = 1; i < ctx->n_samples; i++) {
        sample_group_t tmp = ctx->samples[i];
        int j = i - 1;
        while (j >= 0 && strcasecmp_portable(ctx->samples[j].sample_name,
                                              tmp.sample_name) > 0) {
            ctx->samples[j + 1] = ctx->samples[j];
            j--;
        }
        ctx->samples[j + 1] = tmp;
    }
}

/* ------------------------------------------------------------------ */
/* Worker thread                                                       */
/* ------------------------------------------------------------------ */
static int analysis_worker(void *data) {
    analysis_context_t *ctx = (analysis_context_t *)data;

    /* 1. Load index (once) ------------------------------------------ */
    ctx->state = ANALYSIS_LOADING_INDEX;
    halal_index_t *idx = index_load(ctx->index_path);
    if (!idx) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Failed to load index: %s", ctx->index_path);
        ctx->state = ANALYSIS_ERROR;
        return 1;
    }

    /* Allocate reports array */
    ctx->reports = (halal_report_t **)hs_calloc(
        (size_t)ctx->n_samples, sizeof(halal_report_t *));
    ctx->n_reports = ctx->n_samples;

    /* Pre-compute amplicon lengths (shared across samples) */
    int *amp_lens = (int *)hs_calloc(
        (size_t)(idx->db->n_species * idx->db->n_markers), sizeof(int));
    for (int i = 0; i < idx->db->n_marker_refs; i++) {
        int si = idx->db->markers[i].species_idx;
        int mi = idx->db->markers[i].marker_idx;
        amp_lens[si * idx->db->n_markers + mi] =
            idx->db->markers[i].amplicon_length;
    }

    /* Mito copy number priors (shared across samples) */
    double *mito_cn = (double *)hs_malloc(
        (size_t)idx->db->n_species * sizeof(double));
    for (int s = 0; s < idx->db->n_species; s++)
        mito_cn[s] = idx->db->species[s].mito_copy_number;

    /* 2. Per-sample loop -------------------------------------------- */
    for (int si = 0; si < ctx->n_samples; si++) {
        sample_group_t *sg = &ctx->samples[si];
        ctx->progress_sample_idx = si;
        ctx->progress_reads = 0;
        ctx->progress_total = 0;

        /* 2a. Read FASTQ for this sample ----------------------------- */
        ctx->state = ANALYSIS_READING_FASTQ;
        int cap = 16384;
        char **seqs  = (char **)hs_malloc((size_t)cap * sizeof(char *));
        char **names = (char **)hs_malloc((size_t)cap * sizeof(char *));
        int  *lens   = (int *)hs_malloc((size_t)cap * sizeof(int));
        int   n_reads = 0;
        int   subsample_hit = 0;

        for (int fi = 0; fi < sg->n_files && !subsample_hit; fi++) {
            int file_idx = sg->file_indices[fi];
            ctx->progress_file_idx = file_idx;
            hs_seqfile_t *sf = hs_seqfile_open(ctx->fastq_paths[file_idx]);
            if (!sf) {
                snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                         "Failed to read: %s", ctx->fastq_paths[file_idx]);
                for (int j = 0; j < n_reads; j++) {
                    free(seqs[j]); free(names[j]);
                }
                free(seqs); free(names); free(lens);
                /* Free already-generated reports */
                for (int k = 0; k < si; k++) {
                    if (ctx->reports[k]) report_destroy(ctx->reports[k]);
                }
                free(ctx->reports); ctx->reports = NULL;
                free(amp_lens); free(mito_cn);
                index_destroy(idx);
                ctx->state = ANALYSIS_ERROR;
                return 1;
            }
            hs_seq_t rec;
            while (hs_seqfile_read(sf, &rec) == 0) {
                if (n_reads >= cap) {
                    cap *= 2;
                    seqs  = (char **)hs_realloc(seqs,  (size_t)cap * sizeof(char *));
                    names = (char **)hs_realloc(names, (size_t)cap * sizeof(char *));
                    lens  = (int *)hs_realloc(lens,  (size_t)cap * sizeof(int));
                }
                seqs[n_reads]  = hs_strdup(rec.seq);
                names[n_reads] = hs_strdup(rec.name);
                lens[n_reads]  = rec.seq_len;
                n_reads++;
                ctx->progress_reads = n_reads;
                if (ctx->subsample_enabled &&
                    ctx->subsample_max_reads > 0 &&
                    n_reads >= ctx->subsample_max_reads) {
                    subsample_hit = 1;
                    break;
                }
            }
            hs_seqfile_close(sf);
        }
        ctx->progress_total = n_reads;

        /* 2b. Classify ----------------------------------------------- */
        ctx->state = ANALYSIS_CLASSIFYING;
        classify_opts_t copts = classify_opts_default();
        read_result_t *results = classify_reads(idx, (const char **)seqs, lens,
                                                 n_reads, &copts);
        if (!results) {
            snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                     "Classification failed for sample %s", sg->sample_name);
            hs_fasta_free_all(seqs, names, lens, n_reads);
            for (int k = 0; k < si; k++) {
                if (ctx->reports[k]) report_destroy(ctx->reports[k]);
            }
            free(ctx->reports); ctx->reports = NULL;
            free(amp_lens); free(mito_cn);
            index_destroy(idx);
            ctx->state = ANALYSIS_ERROR;
            return 1;
        }

        /* 2c. EM ----------------------------------------------------- */
        ctx->state = ANALYSIS_RUNNING_EM;
        int n_em_reads = 0;
        em_read_t *em_reads = em_reads_from_classify(results, n_reads,
                                                      &n_em_reads);
        em_config_t ecfg = em_config_default();
        ecfg.mito_copy_numbers = mito_cn;

        em_result_t *em = NULL;
        if (n_em_reads > 0) {
            em = em_fit(em_reads, n_em_reads,
                        idx->db->n_species, idx->db->n_markers,
                        amp_lens, &ecfg);
        }

        /* 2d. Report ------------------------------------------------- */
        ctx->state = ANALYSIS_GENERATING_REPORT;
        halal_report_t *report = NULL;
        if (em) {
            report = report_generate(em, idx->db, results, n_reads, 0.001);
        } else {
            report = (halal_report_t *)hs_calloc(1, sizeof(halal_report_t));
            snprintf(report->sample_id, sizeof(report->sample_id), "%s",
                     sg->sample_name);
            report->verdict = INCONCLUSIVE;
            report->total_reads = n_reads;
            report->classified_reads = 0;
        }
        /* Tag report with sample name */
        snprintf(report->sample_id, sizeof(report->sample_id), "%s",
                 sg->sample_name);
        ctx->reports[si] = report;

        /* Free per-sample intermediaries */
        if (em) em_result_destroy(em);
        em_reads_free(em_reads, n_em_reads);
        classify_results_free(results, n_reads);
        for (int ri = 0; ri < n_reads; ri++) {
            free(seqs[ri]); free(names[ri]);
        }
        free(seqs); free(names); free(lens);
    }

    /* Shared resources cleanup */
    free(amp_lens);
    free(mito_cn);
    index_destroy(idx);

    ctx->state = ANALYSIS_DONE;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
void analysis_init(analysis_context_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = ANALYSIS_IDLE;
    ctx->subsample_max_reads = 500000;
}

int analysis_start(analysis_context_t *ctx) {
    /* Clean up any previous run */
    analysis_cleanup(ctx);

    ctx->state              = ANALYSIS_IDLE;
    ctx->reports            = NULL;
    ctx->n_reports          = 0;
    ctx->selected_sample    = 0;
    ctx->progress_reads     = 0;
    ctx->progress_total     = 0;
    ctx->progress_file_idx  = 0;
    ctx->progress_sample_idx = 0;
    ctx->error_msg[0]       = '\0';

    ctx->thread = SDL_CreateThread(analysis_worker, "analysis", ctx);
    if (!ctx->thread) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Failed to create thread: %s", SDL_GetError());
        ctx->state = ANALYSIS_ERROR;
        return -1;
    }
    return 0;
}

void analysis_cleanup(analysis_context_t *ctx) {
    if (ctx->thread) {
        int status;
        SDL_WaitThread(ctx->thread, &status);
        ctx->thread = NULL;
    }
    if (ctx->reports) {
        for (int i = 0; i < ctx->n_reports; i++) {
            if (ctx->reports[i])
                report_destroy(ctx->reports[i]);
        }
        free(ctx->reports);
        ctx->reports = NULL;
        ctx->n_reports = 0;
    }
}

/* ------------------------------------------------------------------ */
/* Memory estimation                                                   */
/* ------------------------------------------------------------------ */
mem_estimate_t analysis_estimate_memory(const analysis_context_t *ctx) {
    mem_estimate_t est = {0, 0, 0, 0};
    for (int i = 0; i < ctx->n_fastq_files; i++) {
        FILE *f = fopen(ctx->fastq_paths[i], "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fclose(f);
        est.total_file_bytes += sz;
        /* Detect gzip: check filename extension */
        const char *p = ctx->fastq_paths[i];
        int len = (int)strlen(p);
        int is_gz = (len > 3 && strcmp(p + len - 3, ".gz") == 0);
        est.estimated_uncompressed += is_gz ? sz * 4 : sz;
    }
    /* ~300 bytes per read in FASTQ (4 lines: name+seq+plus+qual) */
    est.estimated_reads = (int)(est.estimated_uncompressed / 300);
    /* With per-sample subsampling, cap is per-sample * n_samples.
       But a simple estimate: cap total at n_samples * subsample_max. */
    if (ctx->subsample_enabled && ctx->subsample_max_reads > 0) {
        int n_samp = ctx->n_samples > 0 ? ctx->n_samples : 1;
        int max_total = n_samp * ctx->subsample_max_reads;
        if (est.estimated_reads > max_total)
            est.estimated_reads = max_total;
    }
    /* ~260 bytes RAM per read (seq + name + classify + EM overhead) */
    est.estimated_ram_mb = (int)((long)est.estimated_reads * 260 / (1024 * 1024));
    return est;
}
