#include "classify.h"
#include "params.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

classify_opts_t classify_opts_default(void) {
    return (classify_opts_t){
        .min_containment = PARAM_MIN_CONTAINMENT_SHORT,
        .coarse_threshold = PARAM_COARSE_THRESHOLD_SHORT,
        .is_nanopore = 0,
        .n_threads = 1,
        .read_mode = READ_MODE_AUTO,
        .lr_window_size = PARAM_LR_WINDOW_SIZE,
        .lr_window_stride = PARAM_LR_WINDOW_STRIDE,
    };
}

classify_opts_t classify_opts_nanopore(void) {
    return (classify_opts_t){
        .min_containment = PARAM_MIN_CONTAINMENT_NANO,
        .coarse_threshold = PARAM_COARSE_THRESHOLD_NANO,
        .is_nanopore = 1,
        .n_threads = 1,
        .read_mode = READ_MODE_LONG,
        .lr_window_size = PARAM_LR_WINDOW_SIZE,
        .lr_window_stride = PARAM_LR_WINDOW_STRIDE,
    };
}

classify_opts_t classify_opts_longread(void) {
    return (classify_opts_t){
        .min_containment = PARAM_MIN_CONTAINMENT_LONG,
        .coarse_threshold = PARAM_COARSE_THRESHOLD_LONG,
        .is_nanopore = 0,
        .n_threads = 1,
        .read_mode = READ_MODE_LONG,
        .lr_window_size = PARAM_LR_WINDOW_SIZE,
        .lr_window_stride = PARAM_LR_WINDOW_STRIDE,
    };
}

/* Compare ints for qsort (used by median calculation) */
static int cmp_int(const void *a, const void *b) {
    return (*(const int *)a) - (*(const int *)b);
}

read_mode_t classify_detect_read_mode(const int *lens, int n_reads) {
    if (n_reads == 0) return READ_MODE_SHORT;
    int n = n_reads < 1000 ? n_reads : 1000;
    int *tmp = (int *)hs_malloc((size_t)n * sizeof(int));
    memcpy(tmp, lens, (size_t)n * sizeof(int));
    qsort(tmp, (size_t)n, sizeof(int), cmp_int);
    int median = tmp[n / 2];
    free(tmp);
    return median > PARAM_LR_MODE_THRESHOLD ? READ_MODE_LONG : READ_MODE_SHORT;
}

/* Classify a single window or short read using the LR sub-index.
 * Strategy: coarse screen (k=11) selects top-5 candidates, then
 * fine matching (k=13, species-unique k-mers) with relative threshold
 * ensures only the true species passes. */
static read_result_t classify_one_lr_window(const halal_index_t *idx,
                                             const char *seq, int len,
                                             const classify_opts_t *opts) {
    read_result_t res;
    memset(&res, 0, sizeof(res));
    res.marker_idx = -1;

    int S = idx->db->n_species;
    int M = idx->db->n_markers;

    /* Step 1: Detect marker via LR primer matching (k=9) */
    int marker = index_detect_marker_lr(idx, seq, len);

    /* Step 2: Coarse screen at k=11 — select top 5 candidates */
    double *coarse_scores = (double *)hs_calloc((size_t)S, sizeof(double));
    index_query_coarse_lr(idx, seq, len, coarse_scores, S);

    int top5[5] = {-1, -1, -1, -1, -1};
    double top5_scores[5] = {0};
    for (int s = 0; s < S; s++) {
        if (coarse_scores[s] > 0) {
            int pos = -1;
            for (int t = 0; t < 5; t++) {
                if (coarse_scores[s] > top5_scores[t]) { pos = t; break; }
            }
            if (pos >= 0) {
                for (int t = 4; t > pos; t--) {
                    top5[t] = top5[t-1];
                    top5_scores[t] = top5_scores[t-1];
                }
                top5[pos] = s;
                top5_scores[pos] = coarse_scores[s];
            }
        }
    }
    free(coarse_scores);

    if (top5[0] < 0) return res; /* no coarse candidates */

    /* Step 3: Fine k=13 resolution on top 5 coarse candidates.
     * Uses species-unique k-mers (shared k-mers removed at index build). */
    species_hit_t *hits = (species_hit_t *)hs_malloc((size_t)5 * sizeof(species_hit_t));
    int n_hits = 0;
    double best_fine = 0.0;

    for (int t = 0; t < 5 && top5[t] >= 0; t++) {
        int s = top5[t];
        double max_fine_s = 0.0;

        if (marker >= 0) {
            max_fine_s = index_query_fine_lr(idx, seq, len, marker, s);
        } else {
            for (int m = 0; m < M; m++) {
                double f = index_query_fine_lr(idx, seq, len, m, s);
                if (f > max_fine_s) max_fine_s = f;
            }
        }

        if (max_fine_s > best_fine) best_fine = max_fine_s;
        hits[n_hits].species_idx = s;
        hits[n_hits].containment = max_fine_s;
        n_hits++;
    }

    /* Relative threshold: keep species within 90% of best AND above absolute min.
     * This eliminates noise from conserved k-mers between related species. */
    double rel_thresh = best_fine * 0.90;
    double eff_thresh = rel_thresh > opts->min_containment ? rel_thresh : opts->min_containment;

    int kept = 0;
    for (int i = 0; i < n_hits; i++) {
        if (hits[i].containment >= eff_thresh) {
            hits[kept++] = hits[i];
        }
    }
    n_hits = kept;

    if (n_hits == 0) {
        free(hits);
        return res;
    }

    res.marker_idx = marker;
    res.hits = hits;
    res.n_hits = n_hits;
    res.is_classified = 1;
    return res;
}

/* Classify a long read using sliding window + LR sub-index */
static read_result_t classify_one_longread(const halal_index_t *idx,
                                            const char *seq, int len,
                                            const classify_opts_t *opts) {
    int win = opts->lr_window_size;
    int stride = opts->lr_window_stride;

    /* Short long-read: classify directly without windowing */
    if (len <= win) {
        return classify_one_lr_window(idx, seq, len, opts);
    }

    int S = idx->db->n_species;
    /* Track max containment per species across windows */
    double *max_cont = (double *)hs_calloc((size_t)S, sizeof(double));
    int *best_marker_per_sp = (int *)hs_malloc((size_t)S * sizeof(int));
    for (int s = 0; s < S; s++) best_marker_per_sp[s] = -1;

    int global_marker = -1;

    /* Slide windows across the read */
    for (int start = 0; start + win <= len; start += stride) {
        read_result_t wres = classify_one_lr_window(idx, seq + start, win, opts);
        if (wres.is_classified) {
            if (global_marker < 0) global_marker = wres.marker_idx;
            for (int h = 0; h < wres.n_hits; h++) {
                int s = wres.hits[h].species_idx;
                if (wres.hits[h].containment > max_cont[s]) {
                    max_cont[s] = wres.hits[h].containment;
                    best_marker_per_sp[s] = wres.marker_idx;
                }
            }
        }
        free(wres.hits);
    }

    /* Also classify any trailing partial window */
    int last_start = ((len - win) / stride) * stride;
    if (last_start + win < len) {
        int tail_start = len - win;
        read_result_t wres = classify_one_lr_window(idx, seq + tail_start, win, opts);
        if (wres.is_classified) {
            if (global_marker < 0) global_marker = wres.marker_idx;
            for (int h = 0; h < wres.n_hits; h++) {
                int s = wres.hits[h].species_idx;
                if (wres.hits[h].containment > max_cont[s]) {
                    max_cont[s] = wres.hits[h].containment;
                    best_marker_per_sp[s] = wres.marker_idx;
                }
            }
        }
        free(wres.hits);
    }

    /* Aggregate: collect species above threshold */
    species_hit_t *hits = (species_hit_t *)hs_malloc((size_t)S * sizeof(species_hit_t));
    int n_hits = 0;
    for (int s = 0; s < S; s++) {
        if (max_cont[s] >= opts->min_containment) {
            hits[n_hits].species_idx = s;
            hits[n_hits].containment = max_cont[s];
            n_hits++;
        }
    }

    free(max_cont);
    free(best_marker_per_sp);

    read_result_t res;
    memset(&res, 0, sizeof(res));
    res.marker_idx = global_marker;

    if (n_hits == 0) {
        free(hits);
        return res;
    }

    res.hits = hits;
    res.n_hits = n_hits;
    res.is_classified = 1;
    return res;
}

static read_result_t classify_one(const halal_index_t *idx,
                                   const char *seq, int len,
                                   const classify_opts_t *opts) {
    read_result_t res;
    memset(&res, 0, sizeof(res));
    res.marker_idx = -1;

    int S = idx->db->n_species;
    int M = idx->db->n_markers;

    /* Step 1: Detect marker via primer matching */
    int marker = index_detect_marker(idx, seq, len);

    /* Step 2: Coarse screen -- get candidate species.
     * For short reads (amplicon data), the FracMinHash sketch has too few
     * hashes to be reliable. Skip coarse screening and try all species. */
    int *is_candidate = (int *)hs_calloc((size_t)S, sizeof(int));
    int n_candidates = 0;
    int n_expected_hashes = (int)((double)(len - idx->coarse_k + 1) * idx->coarse_scale);
    if (n_expected_hashes <= 10) {
        /* Short read: skip coarse, all species are candidates */
        for (int s = 0; s < S; s++) is_candidate[s] = 1;
        n_candidates = S;
    } else {
        double *coarse_scores = (double *)hs_calloc((size_t)S, sizeof(double));
        index_query_coarse(idx, seq, len, coarse_scores, S);
        for (int s = 0; s < S; s++) {
            if (coarse_scores[s] >= opts->coarse_threshold) {
                is_candidate[s] = 1;
                n_candidates++;
            }
        }
        free(coarse_scores);
        if (n_candidates == 0) {
            free(is_candidate);
            return res;
        }
    }

    /* Step 3: Fine resolution for candidates */
    species_hit_t *hits = (species_hit_t *)hs_malloc((size_t)S * sizeof(species_hit_t));
    int n_hits = 0;

    for (int s = 0; s < S; s++) {
        if (!is_candidate[s]) continue;

        double best_fine = 0.0;
        int best_marker = marker;

        if (marker >= 0) {
            /* Marker known: query fine only for that marker */
            best_fine = index_query_fine(idx, seq, len, marker, s);
        } else {
            /* Marker unknown: try all markers, take best */
            for (int m = 0; m < M; m++) {
                double f = index_query_fine(idx, seq, len, m, s);
                if (f > best_fine) { best_fine = f; best_marker = m; }
            }
        }

        if (best_fine >= opts->min_containment) {
            hits[n_hits].species_idx = s;
            hits[n_hits].containment = best_fine;
            n_hits++;
            if (marker < 0) marker = best_marker;
        }
    }

    free(is_candidate);

    if (n_hits == 0) {
        free(hits);
        return res;
    }

    res.marker_idx = marker;
    res.hits = hits;
    res.n_hits = n_hits;
    res.is_classified = 1;
    return res;
}

read_result_t *classify_reads(const halal_index_t *idx,
                               const char **seqs, const int *lens, int n_reads,
                               const classify_opts_t *opts) {
    read_result_t *results = (read_result_t *)hs_calloc((size_t)n_reads, sizeof(read_result_t));

    /* Determine effective read mode and opts */
    const classify_opts_t *eff_opts = opts;
    classify_opts_t lr_opts;
    read_mode_t mode = opts->read_mode;

    if (mode == READ_MODE_AUTO) {
        mode = classify_detect_read_mode(lens, n_reads);
        if (mode == READ_MODE_LONG && idx->has_longread_index) {
            int n = n_reads < 1000 ? n_reads : 1000;
            int *tmp = (int *)hs_malloc((size_t)n * sizeof(int));
            memcpy(tmp, lens, (size_t)n * sizeof(int));
            qsort(tmp, (size_t)n, sizeof(int), cmp_int);
            int median = tmp[n / 2];
            free(tmp);
            HS_LOG_INFO("Auto-detected long reads (median %d bp), switching to longread parameters", median);
            /* Switch to longread-tuned thresholds */
            lr_opts = classify_opts_longread();
            eff_opts = &lr_opts;
        }
    }

    int use_lr = (mode == READ_MODE_LONG) && idx->has_longread_index;

    for (int r = 0; r < n_reads; r++) {
        if (use_lr) {
            results[r] = classify_one_longread(idx, seqs[r], lens[r], eff_opts);
        } else {
            results[r] = classify_one(idx, seqs[r], lens[r], eff_opts);
        }
    }

    return results;
}

void classify_results_free(read_result_t *results, int n) {
    if (!results) return;
    for (int i = 0; i < n; i++) free(results[i].hits);
    free(results);
}

void classify_summarize(const read_result_t *results, int n,
                        const halal_index_t *idx, classify_summary_t *summary) {
    (void)idx;
    memset(summary, 0, sizeof(*summary));
    summary->total_reads = n;
    for (int i = 0; i < n; i++) {
        if (!results[i].is_classified) continue;
        summary->classified_reads++;
        if (results[i].marker_idx >= 0)
            summary->per_marker[results[i].marker_idx]++;
        for (int j = 0; j < results[i].n_hits; j++) {
            int s = results[i].hits[j].species_idx;
            if (s >= 0 && s < HS_MAX_SPECIES) summary->per_species[s]++;
        }
    }
}
