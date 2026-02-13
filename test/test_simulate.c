#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "refdb.h"
#include "simulate.h"
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

static void test_simulate_basic(void) {
    printf("  test_simulate_basic...\n");
    halal_refdb_t *db = refdb_build_default();

    sim_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.n_species = db->n_species;
    cfg.composition = (double *)calloc((size_t)db->n_species, sizeof(double));
    int beef = refdb_find_species(db, "Bos_taurus");
    int pork = refdb_find_species(db, "Sus_scrofa");
    cfg.composition[beef] = 0.9;
    cfg.composition[pork] = 0.1;
    cfg.reads_per_marker = 100;
    cfg.error_rate = 0.001;
    cfg.read_length = 150;
    cfg.seed = 42;

    sim_result_t *sr = simulate_mixture(&cfg, db);
    ASSERT(sr != NULL, "Simulation result created");
    ASSERT(sr->n_reads > 0, "Generated reads");
    /* sr->n_reads should be 300 if both beef and pork have all 3 markers.
     * With new species, we might have fewer if some are missing.
     * But Bos_taurus and Sus_scrofa have COI (if found), cytb, 16S.
     * Wait, Sus_scrofa COI was NOT FOUND in my last run. 
     * So total reads will be 100(COI_beef) + 100(cytb_both) + 100(16S_both) = 300.
     * Actually rpm=100 per marker, so it should be 300.
     */
    ASSERT(sr->n_reads == 300, "300 reads generated");

    /* Check that reads are valid DNA */
    for (int i = 0; i < sr->n_reads; i++) {
        ASSERT(sr->reads[i] != NULL, "Read is not NULL");
        ASSERT(sr->read_lengths[i] > 0, "Read length > 0");
        int valid = 1;
        for (int j = 0; j < sr->read_lengths[i]; j++) {
            char c = sr->reads[i][j];
            if (c != 'A' && c != 'C' && c != 'G' && c != 'T') {
                valid = 0; break;
            }
        }
        ASSERT(valid, "Read is valid DNA");
        if (!valid) break;
    }

    /* Check species distribution roughly matches */
    int beef_count = 0, pork_count = 0;
    for (int i = 0; i < sr->n_reads; i++) {
        if (sr->true_species[i] == beef) beef_count++;
        if (sr->true_species[i] == pork) pork_count++;
    }
    double beef_frac = (double)beef_count / sr->n_reads;
    double pork_frac = (double)pork_count / sr->n_reads;
    ASSERT_NEAR(beef_frac, 0.9, 0.15, "Beef fraction near 0.9");
    ASSERT_NEAR(pork_frac, 0.1, 0.15, "Pork fraction near 0.1");

    sim_result_destroy(sr);
    free(cfg.composition);
    refdb_destroy(db);
}

static void test_simulate_markers(void) {
    printf("  test_simulate_markers...\n");
    halal_refdb_t *db = refdb_build_default();

    sim_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.n_species = db->n_species;
    cfg.composition = (double *)calloc((size_t)db->n_species, sizeof(double));
    int beef = refdb_find_species(db, "Bos_taurus");
    cfg.composition[beef] = 1.0;
    cfg.reads_per_marker = 50;
    cfg.error_rate = 0.0;
    cfg.read_length = 150;
    cfg.seed = 99;

    sim_result_t *sr = simulate_mixture(&cfg, db);
    ASSERT(sr != NULL, "Simulation result created");

    /* All 3 markers should be represented IF they exist in DB for beef */
    int marker_counts[3] = {0};
    for (int i = 0; i < sr->n_reads; i++) {
        int m = sr->true_marker[i];
        if (m >= 0 && m < 3) marker_counts[m]++;
    }
    
    for (int m = 0; m < 3; m++) {
        if (refdb_get_marker_ref(db, beef, m)) {
            char msg[64];
            snprintf(msg, sizeof(msg), "50 reads for marker %d", m);
            ASSERT(marker_counts[m] == 50, msg);
        }
    }

    sim_result_destroy(sr);
    free(cfg.composition);
    refdb_destroy(db);
}

static void test_simulate_deterministic(void) {
    printf("  test_simulate_deterministic...\n");
    halal_refdb_t *db1 = refdb_build_default();
    halal_refdb_t *db2 = refdb_build_default();

    sim_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.n_species = db1->n_species;
    cfg.composition = (double *)calloc((size_t)db1->n_species, sizeof(double));
    int beef = refdb_find_species(db1, "Bos_taurus");
    cfg.composition[beef] = 1.0;
    cfg.reads_per_marker = 10;
    cfg.error_rate = 0.001;
    cfg.read_length = 100;
    cfg.seed = 42;

    sim_result_t *sr1 = simulate_mixture(&cfg, db1);
    sim_result_t *sr2 = simulate_mixture(&cfg, db2);

    ASSERT(sr1->n_reads == sr2->n_reads, "Same number of reads");
    for (int i = 0; i < sr1->n_reads && i < sr2->n_reads; i++) {
        ASSERT(strcmp(sr1->reads[i], sr2->reads[i]) == 0,
               "Same seed produces same reads");
        if (strcmp(sr1->reads[i], sr2->reads[i]) != 0) break;
    }

    sim_result_destroy(sr1);
    sim_result_destroy(sr2);
    free(cfg.composition);
    refdb_destroy(db1);
    refdb_destroy(db2);
}

static void test_simulate_write_fastq(void) {
    printf("  test_simulate_write_fastq...\n");
    halal_refdb_t *db = refdb_build_default();

    sim_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.n_species = db->n_species;
    cfg.composition = (double *)calloc((size_t)db->n_species, sizeof(double));
    int beef = refdb_find_species(db, "Bos_taurus");
    cfg.composition[beef] = 1.0;
    cfg.reads_per_marker = 5;
    cfg.error_rate = 0.0;
    cfg.read_length = 50;
    cfg.seed = 42;

    sim_result_t *sr = simulate_mixture(&cfg, db);

    const char *path = "/tmp/test_sim.fq";
    ASSERT(sim_write_fastq(sr, path) == 0, "Write FASTQ succeeded");

    /* Verify file exists and has content */
    FILE *fp = fopen(path, "r");
    ASSERT(fp != NULL, "FASTQ file exists");
    if (fp) {
        char line[1024];
        ASSERT(fgets(line, sizeof(line), fp) != NULL, "Has first line");
        ASSERT(line[0] == '@', "First line starts with @");
        fclose(fp);
    }

    sim_result_destroy(sr);
    free(cfg.composition);
    refdb_destroy(db);
    remove(path);
}

int main(void) {
    printf("=== test_simulate ===\n");
    test_simulate_basic();
    test_simulate_markers();
    test_simulate_deterministic();
    test_simulate_write_fastq();
    printf("=== %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
