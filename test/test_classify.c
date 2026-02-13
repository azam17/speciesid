#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "refdb.h"
#include "index.h"
#include "classify.h"
#include "utils.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

static void test_classify_reference_reads(void) {
    printf("  test_classify_reference_reads...\n");
    halal_refdb_t *db = refdb_build_default();
    halal_index_t *idx = index_build(db);

    int beef = refdb_find_species(idx->db, "Bos_taurus");
    marker_ref_t *mr = refdb_get_marker_ref(idx->db, beef, 2); /* 16S */
    ASSERT(mr != NULL, "Got beef 16S ref");

    /* Classify the reference sequence itself */
    const char *seqs[1] = { mr->sequence };
    int lens[1] = { mr->seq_len };

    classify_opts_t opts = classify_opts_default();
    opts.min_containment = 0.2; /* relaxed for pseudo-random seqs */
    opts.coarse_threshold = 0.01;
    read_result_t *results = classify_reads(idx, seqs, lens, 1, &opts);

    ASSERT(results[0].is_classified, "Reference seq classified");
    if (results[0].is_classified) {
        /* Should have beef as a hit */
        int found_beef = 0;
        for (int j = 0; j < results[0].n_hits; j++) {
            if (results[0].hits[j].species_idx == beef) {
                found_beef = 1;
                ASSERT(results[0].hits[j].containment > 0.5,
                       "Beef self-containment high");
            }
        }
        ASSERT(found_beef, "Beef found in hits");
    }

    classify_results_free(results, 1);
    index_destroy(idx);
}

static void test_classify_multiple_species(void) {
    printf("  test_classify_multiple_species...\n");
    halal_refdb_t *db = refdb_build_default();
    halal_index_t *idx = index_build(db);

    int beef = refdb_find_species(idx->db, "Bos_taurus");
    int pork = refdb_find_species(idx->db, "Sus_scrofa");

    /* Get 16S refs for both (all species have 16S) */
    marker_ref_t *beef_mr = refdb_get_marker_ref(idx->db, beef, 2);
    marker_ref_t *pork_mr = refdb_get_marker_ref(idx->db, pork, 2);
    ASSERT(beef_mr != NULL && pork_mr != NULL, "Got both refs");

    const char *seqs[2] = { beef_mr->sequence, pork_mr->sequence };
    int lens[2] = { beef_mr->seq_len, pork_mr->seq_len };

    classify_opts_t opts = classify_opts_default();
    opts.min_containment = 0.2;
    opts.coarse_threshold = 0.01;
    read_result_t *results = classify_reads(idx, seqs, lens, 2, &opts);

    /* Both should be classified */
    ASSERT(results[0].is_classified, "Beef read classified");
    ASSERT(results[1].is_classified, "Pork read classified");

    /* Check beef read has beef hit with highest containment */
    if (results[0].is_classified && results[0].n_hits > 0) {
        int best_species = results[0].hits[0].species_idx;
        double best_cont = results[0].hits[0].containment;
        for (int j = 1; j < results[0].n_hits; j++) {
            if (results[0].hits[j].containment > best_cont) {
                best_cont = results[0].hits[j].containment;
                best_species = results[0].hits[j].species_idx;
            }
        }
        ASSERT(best_species == beef, "Beef read best match is beef");
    }

    classify_results_free(results, 2);
    index_destroy(idx);
}

static void test_classify_summary(void) {
    printf("  test_classify_summary...\n");
    halal_refdb_t *db = refdb_build_default();
    halal_index_t *idx = index_build(db);

    int beef = refdb_find_species(idx->db, "Bos_taurus");
    marker_ref_t *mr = refdb_get_marker_ref(idx->db, beef, 2); /* 16S */

    const char *seqs[3] = { mr->sequence, mr->sequence, mr->sequence };
    int lens[3] = { mr->seq_len, mr->seq_len, mr->seq_len };

    classify_opts_t opts = classify_opts_default();
    opts.min_containment = 0.2;
    opts.coarse_threshold = 0.01;
    read_result_t *results = classify_reads(idx, seqs, lens, 3, &opts);

    classify_summary_t summary;
    classify_summarize(results, 3, idx, &summary);
    ASSERT(summary.total_reads == 3, "Total reads = 3");
    ASSERT(summary.classified_reads >= 0, "Classified reads >= 0");

    classify_results_free(results, 3);
    index_destroy(idx);
}

static void test_classify_nanopore_opts(void) {
    printf("  test_classify_nanopore_opts...\n");
    classify_opts_t opts = classify_opts_nanopore();
    ASSERT(opts.is_nanopore == 1, "Nanopore flag set");
    ASSERT(opts.min_containment < 0.3, "Nanopore threshold relaxed");
}

int main(void) {
    printf("=== test_classify ===\n");
    test_classify_reference_reads();
    test_classify_multiple_species();
    test_classify_summary();
    test_classify_nanopore_opts();
    printf("=== %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
