#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "refdb.h"
#include "index.h"
#include "classify.h"
#include "em.h"
#include "report.h"
#include "simulate.h"
#include "degrade.h"
#include "calibrate.h"
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

/* End-to-end: simulate -> classify -> quantify -> report */
static void test_e2e_pipeline(void) {
    printf("  test_e2e_pipeline...\n");

    /* 1. Build database and index */
    halal_refdb_t *db = refdb_build_default();
    halal_refdb_t *db2 = refdb_build_default();
    halal_index_t *idx = index_build(db2);
    ASSERT(idx != NULL, "Index built");

    /* 2. Simulate reads: 80% beef, 20% pork */
    sim_config_t scfg;
    memset(&scfg, 0, sizeof(scfg));
    scfg.n_species = db->n_species;
    scfg.composition = (double *)calloc((size_t)db->n_species, sizeof(double));
    int beef = refdb_find_species(db, "Bos_taurus");
    int pork = refdb_find_species(db, "Sus_scrofa");
    scfg.composition[beef] = 0.8;
    scfg.composition[pork] = 0.2;
    scfg.reads_per_marker = 200;
    scfg.error_rate = 0.001;
    scfg.read_length = 150;
    scfg.seed = 42;

    sim_result_t *sr = simulate_mixture(&scfg, db);
    ASSERT(sr->n_reads > 0, "Simulated reads generated");

    /* 3. Classify reads */
    classify_opts_t copts = classify_opts_default();
    copts.min_containment = 0.2;
    copts.coarse_threshold = 0.01;
    read_result_t *results = classify_reads(idx,
        (const char **)sr->reads, sr->read_lengths, sr->n_reads, &copts);

    int classified = 0;
    for (int i = 0; i < sr->n_reads; i++)
        if (results[i].is_classified) classified++;
    ASSERT(classified > 0, "Some reads classified");

    /* 4. Run EM */
    int n_em_reads;
    em_read_t *em_reads = em_reads_from_classify(results, sr->n_reads, &n_em_reads);

    em_config_t ecfg = em_config_default();
    ecfg.n_restarts = 3;
    int n_sp = idx->db->n_species;
    int n_mk = idx->db->n_markers;
    int *amp_lens = (int *)calloc((size_t)(n_sp * n_mk), sizeof(int));
    for (int i = 0; i < idx->db->n_marker_refs; i++) {
        int si = idx->db->markers[i].species_idx;
        int mi = idx->db->markers[i].marker_idx;
        amp_lens[si * n_mk + mi] = idx->db->markers[i].amplicon_length;
    }

    em_result_t *em = NULL;
    if (n_em_reads > 0) {
        em = em_fit(em_reads, n_em_reads, n_sp, n_mk, amp_lens, &ecfg);
        ASSERT(em != NULL, "EM converged");
    }

    /* 5. Generate report */
    if (em) {
        halal_report_t *report = report_generate(em, idx->db, results, sr->n_reads, 0.001);
        ASSERT(report != NULL, "Report generated");

        /* Verdict should be FAIL (pork detected) */
        ASSERT(report->verdict == FAIL, "Verdict is FAIL (haram detected)");

        /* Pork should be detected above threshold */
        int pork2 = refdb_find_species(idx->db, "Sus_scrofa");
        if (pork2 >= 0) {
            ASSERT(report->species[pork2].weight_pct > 0.1,
                   "Pork weight > 0.1%");
        }

        /* Print report for visual inspection */
        printf("  --- Pipeline Report ---\n");
        report_print_summary(report, stdout);

        report_destroy(report);
    }

    /* Cleanup */
    if (em) em_result_destroy(em);
    em_reads_free(em_reads, n_em_reads);
    classify_results_free(results, sr->n_reads);
    free(amp_lens);
    sim_result_destroy(sr);
    free(scfg.composition);
    index_destroy(idx);
    refdb_destroy(db);
}

/* Test pure halal sample */
static void test_e2e_halal_pass(void) {
    printf("  test_e2e_halal_pass...\n");

    halal_refdb_t *db = refdb_build_default();
    halal_refdb_t *db2 = refdb_build_default();
    halal_index_t *idx = index_build(db2);

    /* Pure beef */
    sim_config_t scfg;
    memset(&scfg, 0, sizeof(scfg));
    scfg.n_species = db->n_species;
    scfg.composition = (double *)calloc((size_t)db->n_species, sizeof(double));
    int beef = refdb_find_species(db, "Bos_taurus");
    scfg.composition[beef] = 1.0;
    scfg.reads_per_marker = 200;
    scfg.error_rate = 0.001;
    scfg.read_length = 150;
    scfg.seed = 99;

    sim_result_t *sr = simulate_mixture(&scfg, db);

    classify_opts_t copts = classify_opts_default();
    copts.min_containment = 0.2;
    copts.coarse_threshold = 0.01;
    read_result_t *results = classify_reads(idx,
        (const char **)sr->reads, sr->read_lengths, sr->n_reads, &copts);

    int n_em_reads;
    em_read_t *em_reads = em_reads_from_classify(results, sr->n_reads, &n_em_reads);

    if (n_em_reads > 0) {
        em_config_t ecfg = em_config_default();
        int n_sp = idx->db->n_species;
        int n_mk = idx->db->n_markers;
        int *amp_lens = (int *)calloc((size_t)(n_sp * n_mk), sizeof(int));
        for (int i = 0; i < idx->db->n_marker_refs; i++) {
            int si = idx->db->markers[i].species_idx;
            int mi = idx->db->markers[i].marker_idx;
            amp_lens[si * n_mk + mi] = idx->db->markers[i].amplicon_length;
        }

        em_result_t *em = em_fit(em_reads, n_em_reads, n_sp, n_mk, amp_lens, &ecfg);
        if (em) {
            halal_report_t *report = report_generate(em, idx->db, results,
                                                      sr->n_reads, 0.001);
            /* Pure beef should pass or be inconclusive (not fail) */
            ASSERT(report->verdict != FAIL, "Pure beef not FAIL");
            report_destroy(report);
            em_result_destroy(em);
        }
        free(amp_lens);
    }

    em_reads_free(em_reads, n_em_reads);
    classify_results_free(results, sr->n_reads);
    sim_result_destroy(sr);
    free(scfg.composition);
    index_destroy(idx);
    refdb_destroy(db);
}

/* Test degradation model */
static void test_degradation(void) {
    printf("  test_degradation...\n");
    int insert_sizes[] = { 200, 180, 220, 190, 210, 150, 250, 170, 230, 195 };
    double lambda = degrade_estimate_lambda(insert_sizes, 10);
    ASSERT(lambda > 0 && lambda < 1.0, "Lambda in reasonable range");

    /* Survival factors */
    int amp_lens[6] = { 658, 425, 560, 658, 425, 560 };
    double surv[6];
    degrade_survival_factors(lambda, amp_lens, 2, 3, surv);
    for (int i = 0; i < 6; i++) {
        ASSERT(surv[i] > 0 && surv[i] <= 1.0, "Survival in [0,1]");
    }
    /* Shorter amplicons should have higher survival */
    ASSERT(surv[1] > surv[0], "Shorter amplicon higher survival");
}

/* Test calibration */
static void test_calibration(void) {
    printf("  test_calibration...\n");
    calibration_sample_t sample;
    double true_w[2] = { 0.7, 0.3 };
    /* Observed reads with some bias */
    double obs[6] = { 500, 480, 510,   /* species 0: ~uniform */
                      300, 250, 350 };  /* species 1: variable */
    sample.true_w = true_w;
    sample.obs_reads = obs;
    sample.n_species = 2;
    sample.n_markers = 3;

    calibration_result_t *cal = calibrate_estimate(&sample, 1);
    ASSERT(cal != NULL, "Calibration succeeded");
    ASSERT(cal->d_sigma > 0, "d_sigma positive");
    ASSERT(cal->b_sigma > 0, "b_sigma positive");

    calibrate_result_destroy(cal);
}

int main(void) {
    printf("=== test_integration ===\n");
    test_e2e_pipeline();
    test_e2e_halal_pass();
    test_degradation();
    test_calibration();
    printf("=== %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
