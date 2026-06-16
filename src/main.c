#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "utils.h"
#include "refdb.h"
#include "index.h"
#include "fastq.h"
#include "classify.h"
#include "em.h"
#include "degrade.h"
#include "calibrate.h"
#include "report.h"
#include "simulate.h"
#include "analyze.h"

static void usage(void) {
    fprintf(stderr,
        "Usage: speciesid <command> [options]\n\n"
        "Commands:\n"
        "  build-db     Build reference database (default built-in species)\n"
        "  index        Build multi-marker index from reference database\n"
        "  run          Full pipeline: classify + quantify + report\n"
        "  analyze      Manifest-driven run for the web workflow\n"
        "  calibrate    Estimate bias priors from spike-in standards\n"
        "  classify     Classify reads against index (no quantification)\n"
        "  quantify     Run EM on pre-classified reads\n"
        "  simulate     Generate synthetic food mixture reads\n"
        "  benchmark    Evaluate on simulated data\n"
        "  version      Print version\n\n"
        "Examples:\n"
        "  speciesid build-db -o speciesid.db\n"
        "  speciesid index -d speciesid.db -o speciesid.idx\n"
        "  speciesid run -x speciesid.idx -r reads.fq.gz -o report.json\n"
        "  speciesid simulate -d speciesid.db -c \"Bos_taurus:0.9,Sus_scrofa:0.1\" -o sim.fq\n"
        "  speciesid benchmark -d speciesid.db -n 100 -o bench.tsv\n"
    );
}

/* --- analyze command (manifest-driven web engine contract) --- */
static int cmd_analyze(int argc, char **argv) {
    const char *manifest_path = NULL;
    const char *output_path = NULL;
    int c;
    static struct option opts[] = {
        { "manifest", required_argument, 0, 'm' },
        { "output", required_argument, 0, 'o' },
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0 }
    };
    while ((c = getopt_long(argc, argv, "m:o:h", opts, NULL)) != -1) {
        switch (c) {
            case 'm': manifest_path = optarg; break;
            case 'o': output_path = optarg; break;
            case 'h': default:
                fprintf(stderr,
                    "Usage: speciesid analyze --manifest manifest.json --output result.json\n"
                    "  Manifest v1 is the stable web-app contract.\n"
                    "  This v1 bridge reports unadjusted screening evidence; control-aware\n"
                    "  correction, out-of-panel, and resolvability models are flagged as\n"
                    "  not implemented unless the C core produces them.\n");
                return c == 'h' ? 0 : 1;
        }
    }
    if (!manifest_path) { HS_LOG_ERROR("No manifest specified (--manifest)"); return 1; }
    if (!output_path) { HS_LOG_ERROR("No output specified (--output)"); return 1; }
    return analyze_execute_manifest(manifest_path, output_path);
}

/* --- build-db command --- */
static int cmd_build_db(int argc, char **argv) {
    const char *output = "speciesid.db";
    const char *fasta_dir = NULL;
    int c;
    static struct option opts[] = {
        { "output", required_argument, 0, 'o' },
        { "fasta-dir", required_argument, 0, 'f' },
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0 }
    };
    while ((c = getopt_long(argc, argv, "o:f:h", opts, NULL)) != -1) {
        switch (c) {
            case 'o': output = optarg; break;
            case 'f': fasta_dir = optarg; break;
            case 'h': default:
                fprintf(stderr, "Usage: speciesid build-db [-f fasta_dir] [-o output.db]\n"
                        "  -f DIR   Build from FASTA files (Species_MARKER.fa)\n"
                        "  -o FILE  Output database file (default: speciesid.db)\n"
                        "  Without -f, builds default database with synthetic sequences\n");
                return c == 'h' ? 0 : 1;
        }
    }

    halal_refdb_t *db;
    if (fasta_dir) {
        HS_LOG_INFO("Building database from FASTA directory: %s", fasta_dir);
        db = refdb_build_from_fasta_dir(fasta_dir);
    } else {
        HS_LOG_INFO("Building default reference database...");
        db = refdb_build_default();
    }
    if (!db) { HS_LOG_ERROR("Failed to build database"); return 1; }

    if (refdb_save(db, output) < 0) {
        HS_LOG_ERROR("Failed to save database to %s", output);
        refdb_destroy(db);
        return 1;
    }

    HS_LOG_INFO("Saved database: %d species, %d markers -> %s",
                db->n_species, db->n_markers, output);
    refdb_destroy(db);
    return 0;
}

/* --- index command --- */
static int cmd_index(int argc, char **argv) {
    const char *db_path = "speciesid.db";
    const char *output = "speciesid.idx";
    int c;
    static struct option opts[] = {
        { "db", required_argument, 0, 'd' },
        { "output", required_argument, 0, 'o' },
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0 }
    };
    while ((c = getopt_long(argc, argv, "d:o:h", opts, NULL)) != -1) {
        switch (c) {
            case 'd': db_path = optarg; break;
            case 'o': output = optarg; break;
            case 'h': default:
                fprintf(stderr, "Usage: speciesid index -d db.db -o output.idx\n");
                return c == 'h' ? 0 : 1;
        }
    }

    halal_refdb_t *db = refdb_load(db_path);
    if (!db) { HS_LOG_ERROR("Failed to load database from %s", db_path); return 1; }

    halal_index_t *idx = index_build(db);
    if (!idx) { HS_LOG_ERROR("Failed to build index"); refdb_destroy(db); return 1; }

    if (index_save(idx, output) < 0) {
        HS_LOG_ERROR("Failed to save index to %s", output);
        index_destroy(idx);
        return 1;
    }

    HS_LOG_INFO("Saved index -> %s", output);
    index_destroy(idx);
    return 0;
}

/* --- calibrate command --- */
static int cmd_calibrate(int argc, char **argv) {
    const char *db_path = "speciesid.db";
    const char *tsv_path = NULL;
    const char *output = "calibration.cal";
    int c;
    static struct option opts[] = {
        { "db", required_argument, 0, 'd' },
        { "spikein", required_argument, 0, 's' },
        { "output", required_argument, 0, 'o' },
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0 }
    };
    while ((c = getopt_long(argc, argv, "d:s:o:h", opts, NULL)) != -1) {
        switch (c) {
            case 'd': db_path = optarg; break;
            case 's': tsv_path = optarg; break;
            case 'o': output = optarg; break;
            case 'h': default:
                fprintf(stderr,
                    "Usage: speciesid calibrate -d db.db -s spikein.tsv [-o calibration.cal]\n"
                    "  -d FILE  Reference database\n"
                    "  -s FILE  Spike-in data TSV (sample_id, species_idx, marker_idx, true_w, obs_reads)\n"
                    "  -o FILE  Output calibration file (default: calibration.cal)\n");
                return c == 'h' ? 0 : 1;
        }
    }

    if (!tsv_path) { HS_LOG_ERROR("No spike-in TSV specified (-s)"); return 1; }

    halal_refdb_t *db = refdb_load(db_path);
    if (!db) { HS_LOG_ERROR("Failed to load database from %s", db_path); return 1; }

    int n_samples;
    calibration_sample_t *samples = calibrate_load_tsv(tsv_path, db->n_species,
                                                        db->n_markers, &n_samples);
    if (!samples || n_samples == 0) {
        HS_LOG_ERROR("Failed to load spike-in data from %s", tsv_path);
        refdb_destroy(db);
        return 1;
    }

    HS_LOG_INFO("Loaded %d calibration samples", n_samples);

    calibration_result_t *cal = calibrate_estimate(samples, n_samples);
    if (!cal) {
        HS_LOG_ERROR("Calibration estimation failed");
        for (int i = 0; i < n_samples; i++) {
            free(samples[i].true_w);
            free(samples[i].obs_reads);
        }
        free(samples);
        refdb_destroy(db);
        return 1;
    }

    HS_LOG_INFO("Calibration: d_mu=%.4f d_sigma=%.4f b_mu=%.4f b_sigma=%.4f",
                cal->d_mu, cal->d_sigma, cal->b_mu, cal->b_sigma);

    if (calibrate_save(cal, output) < 0) {
        HS_LOG_ERROR("Failed to save calibration to %s", output);
    } else {
        HS_LOG_INFO("Saved calibration -> %s", output);
    }

    calibrate_result_destroy(cal);
    for (int i = 0; i < n_samples; i++) {
        free(samples[i].true_w);
        free(samples[i].obs_reads);
    }
    free(samples);
    refdb_destroy(db);
    return 0;
}

/* --- run command (full pipeline) --- */
static int cmd_run(int argc, char **argv) {
    const char *idx_path = "speciesid.idx";
    const char *reads_path = NULL;
    const char *output = NULL;
    const char *format = "summary";
    const char *cal_path = NULL;
    double threshold = 0.001;
    double prune_threshold = 0.0;
    int is_nanopore = 0;
    int is_longread = 0;
    int use_degradation = 0;
    int use_advanced = 0;
    int use_fast = 0;
    int use_fisher_ci = 0;
    int use_brent_lambda = 0;
    int use_full_lrt = 0;
    int n_bootstrap = 0;
    int c;
    static struct option opts[] = {
        { "index", required_argument, 0, 'x' },
        { "reads", required_argument, 0, 'r' },
        { "output", required_argument, 0, 'o' },
        { "format", required_argument, 0, 'f' },
        { "threshold", required_argument, 0, 't' },
        { "nanopore", no_argument, 0, 'n' },
        { "longread", no_argument, 0, 'L' },
        { "calibration", required_argument, 0, 'c' },
        { "degradation", no_argument, 0, 'D' },
        { "prune", required_argument, 0, 'P' },
        { "advanced", no_argument, 0, 'A' },
        { "fast", no_argument, 0, 1000 },
        { "fisher-ci", no_argument, 0, 1001 },
        { "brent-lambda", no_argument, 0, 1002 },
        { "full-lrt", no_argument, 0, 1003 },
        { "bootstrap", required_argument, 0, 1004 },
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0 }
    };
    while ((c = getopt_long(argc, argv, "x:r:o:f:t:nLc:DP:Ah", opts, NULL)) != -1) {
        switch (c) {
            case 'x': idx_path = optarg; break;
            case 'r': reads_path = optarg; break;
            case 'o': output = optarg; break;
            case 'f': format = optarg; break;
            case 't': threshold = atof(optarg); break;
            case 'n': is_nanopore = 1; break;
            case 'L': is_longread = 1; break;
            case 'c': cal_path = optarg; break;
            case 'D': use_degradation = 1; break;
            case 'P': prune_threshold = atof(optarg); break;
            case 'A': use_advanced = 1; break;
            case 1000: use_fast = 1; break;
            case 1001: use_fisher_ci = 1; break;
            case 1002: use_brent_lambda = 1; break;
            case 1003: use_full_lrt = 1; break;
            case 1004: n_bootstrap = atoi(optarg); break;
            case 'h': default:
                fprintf(stderr,
                    "Usage: speciesid run -x index.idx -r reads.fq [-o report] [-f json|tsv|summary]\n"
                    "  -L, --longread      Force long-read mode (Nanopore/PacBio)\n"
                    "  -n, --nanopore      Legacy nanopore mode (use --longread instead)\n"
                    "  --calibration FILE  Load calibration priors from file\n"
                    "  --degradation       Enable degradation modeling\n"
                    "  --prune FLOAT       Post-EM pruning threshold (remove species below this weight)\n"
                    "  --advanced          Enable Brent lambda (already default: Louis CI + full LRT)\n"
                    "  --fast              Use fast Wald CIs + profile LRT instead of rigorous defaults\n"
                    "  --fisher-ci         Use observed Fisher information CIs (default, override if --fast)\n"
                    "  --brent-lambda      Use Brent's method for lambda (requires --advanced or explicit)\n"
                    "  --full-lrt          Use full nested-model LRT (default, override if --fast)\n"
                    "  --bootstrap N       Run N bootstrap resamples for stability assessment\n");
                return c == 'h' ? 0 : 1;
        }
    }

    if (!reads_path) { HS_LOG_ERROR("No reads file specified (-r)"); return 1; }

    halal_index_t *idx = index_load(idx_path);
    if (!idx) { HS_LOG_ERROR("Failed to load index from %s", idx_path); return 1; }

    /* Read all sequences */
    char **seqs; char **names; int *lens; int n_reads;
    if (hs_fasta_read_all(reads_path, &seqs, &names, &lens, &n_reads) < 0) {
        HS_LOG_ERROR("Failed to read %s", reads_path);
        index_destroy(idx);
        return 1;
    }
    HS_LOG_INFO("Read %d sequences from %s", n_reads, reads_path);

    /* Classify */
    classify_opts_t copts;
    if (is_longread) {
        copts = classify_opts_longread();
    } else if (is_nanopore) {
        copts = classify_opts_nanopore();
    } else {
        copts = classify_opts_default();  /* READ_MODE_AUTO */
    }
    read_result_t *results = classify_reads(idx, (const char **)seqs, lens, n_reads, &copts);

    /* Build EM input */
    int n_em_reads;
    em_read_t *em_reads = em_reads_from_classify(results, n_reads, &n_em_reads);
    HS_LOG_INFO("Classified %d reads for EM", n_em_reads);
    if (n_em_reads == 0) {
        HS_LOG_WARN("No reads matched any reference marker — output will be empty");
    }

    /* Run EM */
    em_config_t ecfg = em_config_default();
    ecfg.estimate_degradation = use_degradation;
    ecfg.prune_threshold = prune_threshold;
    if (use_fast) {
        ecfg.use_advanced_ci = 0;
        ecfg.use_full_lrt = 0;
    }
    if (use_advanced) {
        ecfg.use_advanced_ci = 1;
        ecfg.use_brent_lambda = 1;
        ecfg.use_full_lrt = 1;
    }
    if (use_fisher_ci) ecfg.use_advanced_ci = 1;
    if (use_brent_lambda) ecfg.use_brent_lambda = 1;
    if (use_full_lrt) ecfg.use_full_lrt = 1;

    /* Load calibration priors if specified */
    if (cal_path) {
        calibration_result_t *cal = calibrate_load(cal_path);
        if (cal) {
            ecfg.d_mu = cal->d_mu;
            ecfg.d_sigma = cal->d_sigma;
            ecfg.b_mu = cal->b_mu;
            ecfg.b_sigma = cal->b_sigma;
            HS_LOG_INFO("Loaded calibration: d_mu=%.4f d_sigma=%.4f b_mu=%.4f b_sigma=%.4f",
                        ecfg.d_mu, ecfg.d_sigma, ecfg.b_mu, ecfg.b_sigma);
            calibrate_result_destroy(cal);
        } else {
            HS_LOG_WARN("Failed to load calibration from %s, using defaults", cal_path);
        }
    }

    /* Pass mito copy numbers from refdb for single-marker bias correction */
    double *mito_cn = (double *)hs_malloc((size_t)idx->db->n_species * sizeof(double));
    for (int s = 0; s < idx->db->n_species; s++)
        mito_cn[s] = idx->db->species[s].mito_copy_number;
    ecfg.mito_copy_numbers = mito_cn;

    int *amp_lens = (int *)hs_calloc(
        (size_t)(idx->db->n_species * idx->db->n_markers), sizeof(int));
    for (int i = 0; i < idx->db->n_marker_refs; i++) {
        int si = idx->db->markers[i].species_idx;
        int mi = idx->db->markers[i].marker_idx;
        amp_lens[si * idx->db->n_markers + mi] = idx->db->markers[i].amplicon_length;
    }

    em_result_t *em = em_fit(em_reads, n_em_reads,
                              idx->db->n_species, idx->db->n_markers,
                              amp_lens, &ecfg);

    /* Generate report */
    halal_report_t *report = report_generate(em, idx->db, results, n_reads, threshold);

    /* Bootstrap stability: re-run pipeline on N subsamples of reads */
    if (n_bootstrap > 0 && n_reads > 0 && n_em_reads > 0) {
        int subsample_n = n_reads / 2 > 100 ? n_reads / 2 : n_reads;
        int stable_count = 0;
        uint64_t bs_seed = 42;

        for (int bs = 0; bs < n_bootstrap; bs++) {
            /* Subsample reads with replacement */
            hs_rng_t bs_rng;
            hs_rng_seed(&bs_rng, bs_seed + (uint64_t)bs * 1000);
            char **bs_seqs = (char **)hs_malloc((size_t)subsample_n * sizeof(char *));
            int *bs_lens = (int *)hs_malloc((size_t)subsample_n * sizeof(int));
            for (int i = 0; i < subsample_n; i++) {
                int ridx = (int)(hs_rng_uniform(&bs_rng) * n_reads);
                bs_seqs[i] = seqs[ridx];
                bs_lens[i] = lens[ridx];
            }

            classify_opts_t bs_copts = copts;
            read_result_t *bs_results = classify_reads(idx,
                (const char **)bs_seqs, bs_lens, subsample_n, &bs_copts);
            int bs_n_em;
            em_read_t *bs_em_reads = em_reads_from_classify(bs_results, subsample_n, &bs_n_em);

            em_config_t bs_ecfg = ecfg;
            bs_ecfg.n_restarts = 1;
            bs_ecfg.max_iter = 100;

            int *bs_amp_lens = (int *)hs_calloc(
                (size_t)(idx->db->n_species * idx->db->n_markers), sizeof(int));
            for (int j = 0; j < idx->db->n_marker_refs; j++) {
                int si = idx->db->markers[j].species_idx;
                int mi = idx->db->markers[j].marker_idx;
                bs_amp_lens[si * idx->db->n_markers + mi] = idx->db->markers[j].amplicon_length;
            }

            double *bs_mito_cn = (double *)hs_malloc((size_t)idx->db->n_species * sizeof(double));
            for (int s = 0; s < idx->db->n_species; s++)
                bs_mito_cn[s] = idx->db->species[s].mito_copy_number;
            bs_ecfg.mito_copy_numbers = bs_mito_cn;

            em_result_t *bs_em = bs_n_em > 0
                ? em_fit(bs_em_reads, bs_n_em, idx->db->n_species,
                         idx->db->n_markers, bs_amp_lens, &bs_ecfg)
                : NULL;

            /* Compare species list with baseline: same species at >0 weight */
            int match = 1;
            if (bs_em) {
                for (int s = 0; s < idx->db->n_species && match; s++) {
                    int in_baseline = (em && em->w[s] > 1e-6);
                    int in_bootstrap = (bs_em->w[s] > 1e-6);
                    if (in_baseline != in_bootstrap) match = 0;
                }
                em_result_destroy(bs_em);
            } else {
                match = 0;
            }

            if (match) stable_count++;

            free(bs_mito_cn);
            free(bs_amp_lens);
            em_reads_free(bs_em_reads, bs_n_em);
            classify_results_free(bs_results, subsample_n);
            free(bs_seqs);
            free(bs_lens);
        }

        report->bootstrap_stability_pct = (int)(100.0 * stable_count / n_bootstrap);
        report->n_bootstrap_resamples = n_bootstrap;
        HS_LOG_INFO("Bootstrap stability: %d/%d (%.1f%%)",
                    stable_count, n_bootstrap,
                    100.0 * stable_count / n_bootstrap);
    }

    FILE *out_fp = stdout;
    if (output) {
        out_fp = fopen(output, "w");
        if (!out_fp) { HS_LOG_ERROR("Cannot open %s for writing", output); out_fp = stdout; }
    }

    if (strcmp(format, "json") == 0) report_print_json(report, out_fp);
    else if (strcmp(format, "tsv") == 0) report_print_tsv(report, out_fp);
    else report_print_summary(report, out_fp);

    if (out_fp != stdout) fclose(out_fp);

    /* Cleanup */
    report_destroy(report);
    em_result_destroy(em);
    em_reads_free(em_reads, n_em_reads);
    classify_results_free(results, n_reads);
    free(amp_lens);
    free(mito_cn);
    hs_fasta_free_all(seqs, names, lens, n_reads);
    index_destroy(idx);
    return 0;
}

/* --- simulate command --- */
static int cmd_simulate(int argc, char **argv) {
    const char *db_path = "speciesid.db";
    const char *composition_str = "Bos_taurus:0.9,Sus_scrofa:0.1";
    const char *output = "-";
    int reads_per_marker = 1000;
    double error_rate = 0.001;
    int read_length = 150;
    uint64_t seed = 42;
    int c;
    static struct option opts[] = {
        { "db", required_argument, 0, 'd' },
        { "composition", required_argument, 0, 'c' },
        { "output", required_argument, 0, 'o' },
        { "reads", required_argument, 0, 'n' },
        { "error-rate", required_argument, 0, 'e' },
        { "read-length", required_argument, 0, 'l' },
        { "seed", required_argument, 0, 's' },
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0 }
    };
    while ((c = getopt_long(argc, argv, "d:c:o:n:e:l:s:h", opts, NULL)) != -1) {
        switch (c) {
            case 'd': db_path = optarg; break;
            case 'c': composition_str = optarg; break;
            case 'o': output = optarg; break;
            case 'n': reads_per_marker = atoi(optarg); break;
            case 'e': error_rate = atof(optarg); break;
            case 'l': read_length = atoi(optarg); break;
            case 's': seed = (uint64_t)atol(optarg); break;
            case 'h': default:
                fprintf(stderr,
                    "Usage: speciesid simulate -d db.db -c \"Species1:frac1,Species2:frac2\" [-o out.fq]\n");
                return c == 'h' ? 0 : 1;
        }
    }

    halal_refdb_t *db = refdb_load(db_path);
    if (!db) { HS_LOG_ERROR("Failed to load database from %s", db_path); return 1; }

    /* Parse composition string */
    sim_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.n_species = db->n_species;
    cfg.composition = (double *)hs_calloc((size_t)db->n_species, sizeof(double));
    cfg.reads_per_marker = reads_per_marker;
    cfg.error_rate = error_rate;
    cfg.read_length = read_length;
    cfg.seed = seed;

    /* Parse "Species1:0.9,Species2:0.1" */
    char *comp = hs_strdup(composition_str);
    char *token = strtok(comp, ",");
    while (token) {
        char *colon = strchr(token, ':');
        if (colon) {
            *colon = '\0';
            double frac = atof(colon + 1);
            int si = refdb_find_species(db, token);
            if (si >= 0) cfg.composition[si] = frac;
            else HS_LOG_WARN("Unknown species: %s", token);
        }
        token = strtok(NULL, ",");
    }
    free(comp);

    sim_result_t *sr = simulate_mixture(&cfg, db);
    HS_LOG_INFO("Simulated %d reads", sr->n_reads);

    sim_write_fastq(sr, output);

    sim_result_destroy(sr);
    free(cfg.composition);
    refdb_destroy(db);
    return 0;
}

/* --- benchmark command --- */
static int cmd_benchmark(int argc, char **argv) {
    const char *db_path = "speciesid.db";
    const char *output = "-";
    int n_mixtures = 10;
    int reads_per_marker = 500;
    uint64_t seed = 123;
    int use_advanced = 0;
    int c;
    static struct option opts[] = {
        { "db", required_argument, 0, 'd' },
        { "output", required_argument, 0, 'o' },
        { "mixtures", required_argument, 0, 'n' },
        { "reads", required_argument, 0, 'r' },
        { "seed", required_argument, 0, 's' },
        { "advanced", no_argument, 0, 'A' },
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0 }
    };
    while ((c = getopt_long(argc, argv, "d:o:n:r:s:Ah", opts, NULL)) != -1) {
        switch (c) {
            case 'd': db_path = optarg; break;
            case 'o': output = optarg; break;
            case 'n': n_mixtures = atoi(optarg); break;
            case 'r': reads_per_marker = atoi(optarg); break;
            case 's': seed = (uint64_t)atol(optarg); break;
            case 'A': use_advanced = 1; break;
            case 'h': default:
                fprintf(stderr,
                    "Usage: speciesid benchmark -d db.db [-n n_mixtures] [-o output.tsv] [--advanced]\n");
                return c == 'h' ? 0 : 1;
        }
    }

    halal_refdb_t *db = refdb_load(db_path);
    if (!db) { HS_LOG_ERROR("Failed to load database from %s", db_path); return 1; }

    FILE *out_fp = stdout;
    if (strcmp(output, "-") != 0) {
        out_fp = fopen(output, "w");
        if (!out_fp) { HS_LOG_ERROR("Cannot open %s", output); refdb_destroy(db); return 1; }
    }

    /* Build index once */
    halal_refdb_t *db2 = refdb_load(db_path);
    halal_index_t *idx = index_build(db2);

    hs_rng_t rng;
    hs_rng_seed(&rng, seed);

    fprintf(out_fp, "mixture\ttrue_pork\test_pork\tabs_error\tscreening_result\n");

    double total_error = 0.0;
    for (int i = 0; i < n_mixtures; i++) {
        /* Random 2-species mixture: beef + pork */
        double pork_frac = hs_rng_uniform(&rng) * 0.3; /* 0-30% */

        sim_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.n_species = db->n_species;
        cfg.composition = (double *)hs_calloc((size_t)db->n_species, sizeof(double));
        int beef_idx = refdb_find_species(db, "Bos_taurus");
        int pork_idx = refdb_find_species(db, "Sus_scrofa");
        if (beef_idx >= 0) cfg.composition[beef_idx] = 1.0 - pork_frac;
        if (pork_idx >= 0) cfg.composition[pork_idx] = pork_frac;
        cfg.reads_per_marker = reads_per_marker;
        cfg.error_rate = 0.001;
        cfg.read_length = 150;
        cfg.seed = seed + (uint64_t)i;

        sim_result_t *sr = simulate_mixture(&cfg, db);

        /* Classify and quantify */
        classify_opts_t copts = classify_opts_default();
        read_result_t *results = classify_reads(idx,
            (const char **)sr->reads, sr->read_lengths, sr->n_reads, &copts);

        int n_em_reads;
        em_read_t *em_reads = em_reads_from_classify(results, sr->n_reads, &n_em_reads);

        em_config_t ecfg = em_config_default();
        if (use_advanced) {
            ecfg.use_advanced_ci = 1;
            ecfg.use_brent_lambda = 1;
            ecfg.use_full_lrt = 1;
        }

        /* Pass mito copy numbers for single-marker bias correction */
        double *mito_cn = (double *)hs_malloc((size_t)idx->db->n_species * sizeof(double));
        for (int s = 0; s < idx->db->n_species; s++)
            mito_cn[s] = idx->db->species[s].mito_copy_number;
        ecfg.mito_copy_numbers = mito_cn;

        int *amp_lens = (int *)hs_calloc(
            (size_t)(idx->db->n_species * idx->db->n_markers), sizeof(int));
        for (int j = 0; j < idx->db->n_marker_refs; j++) {
            int si = idx->db->markers[j].species_idx;
            int mi = idx->db->markers[j].marker_idx;
            amp_lens[si * idx->db->n_markers + mi] = idx->db->markers[j].amplicon_length;
        }

        em_result_t *em = NULL;
        double est_pork = 0.0;
        const char *vstr = "N/A";

        if (n_em_reads > 0) {
            em = em_fit(em_reads, n_em_reads, idx->db->n_species,
                        idx->db->n_markers, amp_lens, &ecfg);
            int pork_idx2 = refdb_find_species(idx->db, "Sus_scrofa");
            if (pork_idx2 >= 0 && em) est_pork = em->w[pork_idx2];

            halal_report_t *rep = report_generate(em, idx->db, results, sr->n_reads, 0.001);
            vstr = screening_result_str(rep->screening_result);
            fprintf(out_fp, "%d\t%.4f\t%.4f\t%.4f\t%s\n",
                    i, pork_frac, est_pork, fabs(est_pork - pork_frac), vstr);
            total_error += fabs(est_pork - pork_frac);
            report_destroy(rep);
        } else {
            fprintf(out_fp, "%d\t%.4f\t%.4f\t%.4f\t%s\n",
                    i, pork_frac, 0.0, pork_frac, "NO_READS");
            total_error += pork_frac;
        }

        if (em) em_result_destroy(em);
        em_reads_free(em_reads, n_em_reads);
        classify_results_free(results, sr->n_reads);
        free(amp_lens);
        free(mito_cn);
        sim_result_destroy(sr);
        free(cfg.composition);
    }

    fprintf(out_fp, "# MAE = %.4f\n", total_error / n_mixtures);
    HS_LOG_INFO("Benchmark: %d mixtures, MAE = %.4f", n_mixtures, total_error / n_mixtures);

    if (out_fp != stdout) fclose(out_fp);
    index_destroy(idx);
    refdb_destroy(db);
    return 0;
}

/* --- Main dispatch --- */
int main(int argc, char **argv) {
    if (argc < 2) { usage(); return 1; }

    const char *cmd = argv[1];
    /* Shift argv for subcommand */
    optind = 1;

    if (strcmp(cmd, "build-db") == 0) return cmd_build_db(argc - 1, argv + 1);
    if (strcmp(cmd, "index") == 0) return cmd_index(argc - 1, argv + 1);
    if (strcmp(cmd, "run") == 0) return cmd_run(argc - 1, argv + 1);
    if (strcmp(cmd, "analyze") == 0) return cmd_analyze(argc - 1, argv + 1);
    if (strcmp(cmd, "calibrate") == 0) return cmd_calibrate(argc - 1, argv + 1);
    if (strcmp(cmd, "simulate") == 0) return cmd_simulate(argc - 1, argv + 1);
    if (strcmp(cmd, "benchmark") == 0) return cmd_benchmark(argc - 1, argv + 1);
    if (strcmp(cmd, "version") == 0) {
        printf("speciesid 0.1.0\n");
        return 0;
    }
    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) { usage(); return 0; }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    usage();
    return 1;
}
