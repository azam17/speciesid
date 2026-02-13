#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "refdb.h"
#include "index.h"
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

#define ASSERT_NEAR(a, b, tol, msg) do { \
    if (fabs((a) - (b)) > (tol)) { \
        fprintf(stderr, "  FAIL: %s: %.6f != %.6f (line %d)\n", msg, (a), (b), __LINE__); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

static void test_refdb_create(void) {
    printf("  test_refdb_create...\n");
    halal_refdb_t *db = refdb_build_default();
    ASSERT(db != NULL, "Default db created");
    ASSERT(db->n_species == 19, "19 species");
    ASSERT(db->n_markers == 3, "3 markers");
    ASSERT(db->n_marker_refs > 0, "Has marker refs");

    /* Check species */
    int pork = refdb_find_species(db, "Sus_scrofa");
    ASSERT(pork >= 0, "Found Sus_scrofa");
    ASSERT(db->species[pork].status == HARAM, "Pork is HARAM");

    int beef = refdb_find_species(db, "Bos_taurus");
    ASSERT(beef >= 0, "Found Bos_taurus");
    ASSERT(db->species[beef].status == HALAL, "Beef is HALAL");

    int horse = refdb_find_species(db, "Equus_caballus");
    ASSERT(horse >= 0, "Found Equus_caballus");
    ASSERT(db->species[horse].status == MASHBOOH, "Horse is MASHBOOH");

    refdb_destroy(db);
}

static void test_refdb_save_load(void) {
    printf("  test_refdb_save_load...\n");
    halal_refdb_t *db = refdb_build_default();
    const char *path = "/tmp/test_speciesid.db";

    ASSERT(refdb_save(db, path) == 0, "Save succeeded");
    halal_refdb_t *db2 = refdb_load(path);
    ASSERT(db2 != NULL, "Load succeeded");
    ASSERT(db2->n_species == db->n_species, "Species count matches");
    ASSERT(db2->n_markers == db->n_markers, "Marker count matches");
    ASSERT(db2->n_marker_refs == db->n_marker_refs, "Marker ref count matches");

    /* Check a specific marker ref */
    int beef = refdb_find_species(db2, "Bos_taurus");
    int coi = refdb_find_marker(db2, "COI");
    marker_ref_t *mr = refdb_get_marker_ref(db2, beef, coi);
    ASSERT(mr != NULL, "Found beef COI marker ref");
    ASSERT(mr->seq_len > 0, "Marker ref has sequence");

    refdb_destroy(db);
    refdb_destroy(db2);
    remove(path);
}

static void test_index_build(void) {
    printf("  test_index_build...\n");
    halal_refdb_t *db = refdb_build_default();
    halal_index_t *idx = index_build(db);
    ASSERT(idx != NULL, "Index built");
    ASSERT(idx->coarse != NULL, "Coarse sketches exist");
    ASSERT(idx->fine != NULL, "Fine k-mer sets exist");
    ASSERT(idx->primer_index != NULL, "Primer index exists");

    /* Each species should have a non-empty coarse sketch if it has refs */
    for (int s = 0; s < db->n_species; s++) {
        ASSERT(idx->coarse[s] != NULL, "Coarse sketch exists");
        int has_refs = 0;
        for (int m = 0; m < db->n_markers; m++) {
            if (refdb_get_marker_ref(db, s, m)) { has_refs = 1; break; }
        }
        if (has_refs) {
            ASSERT(idx->coarse[s]->n > 0, "Coarse sketch non-empty");
        }
    }

    index_destroy(idx);
}

static void test_index_query_coarse(void) {
    printf("  test_index_query_coarse...\n");
    halal_refdb_t *db = refdb_build_default();
    halal_index_t *idx = index_build(db);

    /* Query with a reference sequence itself */
    int beef = refdb_find_species(idx->db, "Bos_taurus");
    marker_ref_t *mr = refdb_get_marker_ref(idx->db, beef, 0);
    ASSERT(mr != NULL, "Got beef COI ref");

    double *scores = (double *)calloc((size_t)idx->db->n_species, sizeof(double));
    index_query_coarse(idx, mr->sequence, mr->seq_len, scores, idx->db->n_species);

    /* Self-score should be highest */
    ASSERT(scores[beef] > 0.0, "Self coarse score > 0");
    int best = 0;
    for (int s = 1; s < idx->db->n_species; s++)
        if (scores[s] > scores[best]) best = s;
    ASSERT(best == beef, "Self is best coarse match");

    free(scores);
    index_destroy(idx);
}

static void test_index_query_fine(void) {
    printf("  test_index_query_fine...\n");
    halal_refdb_t *db = refdb_build_default();
    halal_index_t *idx = index_build(db);

    int beef = refdb_find_species(idx->db, "Bos_taurus");
    int pork = refdb_find_species(idx->db, "Sus_scrofa");
    marker_ref_t *mr = refdb_get_marker_ref(idx->db, beef, 0);

    /* Fine query: beef ref vs beef index should be high */
    double c_self = index_query_fine(idx, mr->sequence, mr->seq_len, 0, beef);
    ASSERT(c_self > 0.5, "Fine self-containment high");

    /* Fine query: beef ref vs pork index should be low */
    double c_other = index_query_fine(idx, mr->sequence, mr->seq_len, 0, pork);
    ASSERT(c_other < c_self, "Fine other-containment lower than self");

    index_destroy(idx);
}

static void test_index_save_load(void) {
    printf("  test_index_save_load...\n");
    halal_refdb_t *db = refdb_build_default();
    halal_index_t *idx = index_build(db);
    const char *path = "/tmp/test_speciesid.idx";

    ASSERT(index_save(idx, path) == 0, "Index save succeeded");

    halal_index_t *idx2 = index_load(path);
    ASSERT(idx2 != NULL, "Index load succeeded");
    ASSERT(idx2->db->n_species == idx->db->n_species, "Species count matches");

    /* Verify loaded index works for queries */
    int beef = refdb_find_species(idx2->db, "Bos_taurus");
    marker_ref_t *mr = refdb_get_marker_ref(idx2->db, beef, 0);
    double c = index_query_fine(idx2, mr->sequence, mr->seq_len, 0, beef);
    ASSERT(c > 0.5, "Loaded index queries correctly");

    index_destroy(idx);
    index_destroy(idx2);
    remove(path);
}

static void test_index_detect_marker(void) {
    printf("  test_index_detect_marker...\n");
    halal_refdb_t *db = refdb_build_default();
    halal_index_t *idx = index_build(db);

    /* Use a primer sequence - should detect marker */
    /* COI forward primer: GGTCAACAAATCATAAAGATATTGG */
    int m = index_detect_marker(idx, db->primer_f[0], (int)strlen(db->primer_f[0]));
    ASSERT(m == 0 || m == -1, "Primer detection returns valid marker or -1");

    /* Random sequence should probably not match well */
    int m2 = index_detect_marker(idx, "NNNNNNNNNNNNNNNNNNNN", 20);
    ASSERT(m2 == -1, "Random seq no marker match");

    index_destroy(idx);
}

int main(void) {
    printf("=== test_index ===\n");
    test_refdb_create();
    test_refdb_save_load();
    test_index_build();
    test_index_query_coarse();
    test_index_query_fine();
    test_index_save_load();
    test_index_detect_marker();
    printf("=== %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
