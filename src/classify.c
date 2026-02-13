#include "classify.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

classify_opts_t classify_opts_default(void) {
    return (classify_opts_t){
        .min_containment = 0.3,
        .coarse_threshold = 0.05,
        .is_nanopore = 0,
        .n_threads = 1,
    };
}

classify_opts_t classify_opts_nanopore(void) {
    return (classify_opts_t){
        .min_containment = 0.1,
        .coarse_threshold = 0.02,
        .is_nanopore = 1,
        .n_threads = 1,
    };
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
    if (n_expected_hashes <= 2) {
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

    for (int r = 0; r < n_reads; r++) {
        results[r] = classify_one(idx, seqs[r], lens[r], opts);
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
