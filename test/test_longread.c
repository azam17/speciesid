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

/* --- Test: read mode detection --- */
static void test_detect_read_mode(void) {
    printf("  test_detect_read_mode...\n");

    /* Short reads (Illumina-like) */
    int short_lens[] = {150, 151, 148, 150, 152, 149, 150, 150};
    read_mode_t mode = classify_detect_read_mode(short_lens, 8);
    ASSERT(mode == READ_MODE_SHORT, "Short reads detected as SHORT");

    /* Long reads (Nanopore-like) */
    int long_lens[] = {1500, 2000, 800, 1200, 3000, 900, 1100, 1800};
    mode = classify_detect_read_mode(long_lens, 8);
    ASSERT(mode == READ_MODE_LONG, "Long reads detected as LONG");

    /* Edge case: empty */
    mode = classify_detect_read_mode(NULL, 0);
    ASSERT(mode == READ_MODE_SHORT, "Empty defaults to SHORT");

    /* Edge case: borderline (median ~300) */
    int border_lens[] = {200, 250, 290, 310, 350, 400};
    mode = classify_detect_read_mode(border_lens, 6);
    ASSERT(mode == READ_MODE_LONG, "Borderline median 310 detected as LONG");

    /* MiFish amplicon case: Nanopore reads ~267bp median */
    int mifish_lens[] = {230, 250, 260, 267, 270, 280, 290, 300};
    mode = classify_detect_read_mode(mifish_lens, 8);
    ASSERT(mode == READ_MODE_LONG, "MiFish amplicon median 268 detected as LONG");

    /* True Illumina: 150bp should still be SHORT */
    int illumina_lens[] = {148, 149, 150, 150, 150, 151, 151, 152};
    mode = classify_detect_read_mode(illumina_lens, 8);
    ASSERT(mode == READ_MODE_SHORT, "Illumina 150bp still SHORT");
}

/* --- Test: LR index build --- */
static void test_lr_index_build(void) {
    printf("  test_lr_index_build...\n");

    halal_refdb_t *db = refdb_build_default();
    halal_index_t *idx = index_build(db);

    ASSERT(idx->has_longread_index == 1, "LR index built");
    ASSERT(idx->lr_coarse_k == 11, "LR coarse k=11");
    ASSERT(idx->lr_fine_k == 13, "LR fine k=13");
    ASSERT(idx->lr_primer_k == 9, "LR primer k=9");
    ASSERT(idx->lr_coarse != NULL, "LR coarse array allocated");
    ASSERT(idx->lr_fine != NULL, "LR fine array allocated");
    ASSERT(idx->lr_primer != NULL, "LR primer array allocated");

    /* Verify LR coarse sketches have hashes */
    int beef = refdb_find_species(db, "Bos_taurus");
    ASSERT(beef >= 0, "Found Bos_taurus");
    ASSERT(idx->lr_coarse[beef]->n > 0, "Beef LR coarse sketch has hashes");

    /* Verify LR primer index has k-mers */
    int has_primer_kmers = 0;
    for (int m = 0; m < db->n_markers; m++) {
        if (idx->lr_primer[m] && idx->lr_primer[m]->n_kmers > 0)
            has_primer_kmers = 1;
    }
    ASSERT(has_primer_kmers, "LR primer index has k-mers");

    index_destroy(idx);
}

/* --- Test: LR index save/load --- */
static void test_lr_index_save_load(void) {
    printf("  test_lr_index_save_load...\n");

    halal_refdb_t *db = refdb_build_default();
    halal_index_t *idx = index_build(db);

    ASSERT(index_save(idx, "/tmp/test_lr.idx") == 0, "LR index saved");

    halal_index_t *idx2 = index_load("/tmp/test_lr.idx");
    ASSERT(idx2 != NULL, "LR index loaded");
    ASSERT(idx2->has_longread_index == 1, "LR flag preserved");
    ASSERT(idx2->lr_coarse_k == 11, "LR coarse k preserved");
    ASSERT(idx2->lr_fine_k == 13, "LR fine k preserved");
    ASSERT(idx2->lr_primer_k == 9, "LR primer k preserved");

    /* Verify coarse sketches match */
    int beef = refdb_find_species(idx->db, "Bos_taurus");
    int beef2 = refdb_find_species(idx2->db, "Bos_taurus");
    ASSERT(beef >= 0 && beef2 >= 0, "Found Bos_taurus in both");
    ASSERT(idx->lr_coarse[beef]->n == idx2->lr_coarse[beef2]->n,
           "LR coarse sketch size matches");

    index_destroy(idx);
    index_destroy(idx2);
    remove("/tmp/test_lr.idx");
}

/* --- Helper: add random substitutions to a sequence --- */
static char *add_noise(const char *seq, int len, double error_rate, uint64_t seed) {
    char *noisy = (char *)malloc((size_t)len + 1);
    memcpy(noisy, seq, (size_t)len);
    noisy[len] = '\0';
    const char bases[] = "ACGT";

    hs_rng_t rng;
    hs_rng_seed(&rng, seed);

    for (int i = 0; i < len; i++) {
        if (hs_rng_uniform(&rng) < error_rate) {
            /* Replace with a different base */
            char orig = noisy[i];
            char replacement;
            do {
                replacement = bases[(int)(hs_rng_uniform(&rng) * 4.0) % 4];
            } while (replacement == orig);
            noisy[i] = replacement;
        }
    }
    return noisy;
}

/* --- Test: classify noisy read using LR path --- */
static void test_lr_classify_noisy(void) {
    printf("  test_lr_classify_noisy...\n");

    halal_refdb_t *db = refdb_build_default();
    halal_index_t *idx = index_build(db);

    int beef = refdb_find_species(db, "Bos_taurus");
    marker_ref_t *mr = refdb_get_marker_ref(db, beef, 1); /* cytb - 358 bp */
    ASSERT(mr != NULL, "Got beef cytb ref");

    /* Add 8% noise to simulate Nanopore errors */
    char *noisy = add_noise(mr->sequence, mr->seq_len, 0.08, 42);

    const char *seqs[1] = { noisy };
    int lens[1] = { mr->seq_len };

    classify_opts_t opts = classify_opts_longread();
    opts.read_mode = READ_MODE_LONG;
    read_result_t *results = classify_reads(idx, seqs, lens, 1, &opts);

    ASSERT(results[0].is_classified, "Noisy read classified via LR path");
    if (results[0].is_classified) {
        /* Check beef is among hits */
        int found_beef = 0;
        for (int j = 0; j < results[0].n_hits; j++) {
            if (results[0].hits[j].species_idx == beef) {
                found_beef = 1;
            }
        }
        ASSERT(found_beef, "Beef found in noisy LR classification");
    }

    classify_results_free(results, 1);
    free(noisy);
    index_destroy(idx);
}

/* --- Test: sliding window classification of a long read --- */
static void test_lr_window_split(void) {
    printf("  test_lr_window_split...\n");

    halal_refdb_t *db = refdb_build_default();
    halal_index_t *idx = index_build(db);

    int beef = refdb_find_species(db, "Bos_taurus");
    marker_ref_t *mr = refdb_get_marker_ref(db, beef, 0); /* COI - 709 bp */
    if (!mr) {
        /* Try cytb if COI not available */
        mr = refdb_get_marker_ref(db, beef, 1);
    }
    ASSERT(mr != NULL, "Got beef reference");

    /* Create a "long read" by concatenating the reference with noise */
    int long_len = 1500;
    char *long_read = (char *)malloc((size_t)long_len + 1);
    /* Fill with the reference sequence, repeating as needed */
    for (int i = 0; i < long_len; i++) {
        long_read[i] = mr->sequence[i % mr->seq_len];
    }
    long_read[long_len] = '\0';

    /* Add 5% noise */
    char *noisy_long = add_noise(long_read, long_len, 0.05, 123);

    const char *seqs[1] = { noisy_long };
    int lens[1] = { long_len };

    classify_opts_t opts = classify_opts_longread();
    opts.read_mode = READ_MODE_LONG;
    read_result_t *results = classify_reads(idx, seqs, lens, 1, &opts);

    ASSERT(results[0].is_classified, "Long noisy read classified via windowing");
    if (results[0].is_classified) {
        int found_beef = 0;
        for (int j = 0; j < results[0].n_hits; j++) {
            if (results[0].hits[j].species_idx == beef) {
                found_beef = 1;
            }
        }
        ASSERT(found_beef, "Beef found in windowed classification");
    }

    classify_results_free(results, 1);
    free(noisy_long);
    free(long_read);
    index_destroy(idx);
}

/* --- Test: longread opts --- */
static void test_longread_opts(void) {
    printf("  test_longread_opts...\n");

    classify_opts_t opts = classify_opts_longread();
    ASSERT(opts.read_mode == READ_MODE_LONG, "Longread mode set");
    ASSERT(opts.min_containment <= 0.02, "LR threshold relaxed for noisy reads");
    ASSERT(opts.lr_window_size == 300, "Default window size 300");
    ASSERT(opts.lr_window_stride == 150, "Default window stride 150");
}

/* --- Test: 12S marker added --- */
static void test_12s_marker(void) {
    printf("  test_12s_marker...\n");

    halal_refdb_t *db = refdb_build_default();
    int m12s = refdb_find_marker(db, "12S");
    ASSERT(m12s >= 0, "12S marker found in database");
    ASSERT(db->n_markers == 4, "Database has 4 markers");

    /* Verify primer is set */
    if (m12s >= 0) {
        ASSERT(strlen(db->primer_f[m12s]) > 0, "12S forward primer set");
        ASSERT(strlen(db->primer_r[m12s]) > 0, "12S reverse primer set");
    }

    refdb_destroy(db);
}

int main(void) {
    printf("=== test_longread ===\n");
    test_detect_read_mode();
    test_lr_index_build();
    test_lr_index_save_load();
    test_lr_classify_noisy();
    test_lr_window_split();
    test_longread_opts();
    test_12s_marker();
    printf("=== %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
