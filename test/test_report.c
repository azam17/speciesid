#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "report.h"
#include "params.h"

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

/* Helper: check if species name appears in a string buffer */
static int buf_contains(const char *buf, const char *needle) {
    return strstr(buf, needle) != NULL;
}

/* Build a mock report with 4 species to test the marker-count filter.
 * Uses PARAM_* values so the test adapts to autoresearch parameter changes.
 *
 *   sp0: MANY markers, above ceiling  -> INCLUDED (always passes both filters)
 *   sp1: 0 markers,  mid weight       -> EXCLUDED (below marker threshold, below ceiling)
 *   sp2: 0 markers,  above ceiling    -> INCLUDED (above weight ceiling, immune to marker filter)
 *   sp3: 0 markers,  below floor      -> EXCLUDED (below weight AND read_pct floor)
 */
static halal_report_t *build_mock_report(void) {
    halal_report_t *r = (halal_report_t *)calloc(1, sizeof(halal_report_t));
    strncpy(r->sample_id, "test_filter", sizeof(r->sample_id) - 1);
    r->screening_result = SCREEN_CLEAR;
    r->total_reads = 1000;
    r->classified_reads = 500;
    r->degradation_lambda = 0.001;
    r->cross_marker_agreement = 1.0;
    r->threshold_wpw = 0.01;
    r->n_species = 4;

    /* sp0: MANY markers, weight above ceiling — always INCLUDED */
    strncpy(r->species[0].species_id, "Bos_taurus", HS_MAX_NAME_LEN - 1);
    r->species[0].species_category = CATEGORY_REFERENCE;
    r->species[0].weight_pct = PARAM_LOW_WT_CEILING + 1.0;  /* above ceiling */
    r->species[0].ci_lo = 1.0;
    r->species[0].ci_hi = 10.0;
    r->species[0].p_value = 0.001;
    r->species[0].read_pct = PARAM_LOW_WT_CEILING + 1.0;
    r->species[0].read_counts[0] = 5;  /* COI */
    r->species[0].read_counts[1] = 3;  /* cytb */
    r->species[0].read_counts[2] = 2;  /* 16S */
    r->species[0].read_counts[3] = 1;  /* 12S */
    r->species[0].n_markers_detected = 4;

    /* sp1: 0 markers, weight below ceiling — EXCLUDED by marker-count filter */
    strncpy(r->species[1].species_id, "Dicentrarchus_labrax", HS_MAX_NAME_LEN - 1);
    r->species[1].species_category = CATEGORY_REFERENCE;
    r->species[1].weight_pct = PARAM_LOW_WT_CEILING - 1.0;  /* below ceiling */
    r->species[1].ci_lo = 1.5;
    r->species[1].ci_hi = 4.5;
    r->species[1].p_value = 0.01;
    r->species[1].read_pct = PARAM_LOW_WT_CEILING - 1.0;
    r->species[1].read_counts[2] = 8;  /* 16S only */
    r->species[1].n_markers_detected = 0;  /* below PARAM_MIN_MARKERS_FOR_LOW_WT */

    /* sp2: 0 markers, weight ABOVE ceiling — INCLUDED (ceiling bypass) */
    strncpy(r->species[2].species_id, "Sus_scrofa", HS_MAX_NAME_LEN - 1);
    r->species[2].species_category = CATEGORY_EXCLUSION;
    r->species[2].weight_pct = PARAM_LOW_WT_CEILING + 1.0;
    r->species[2].ci_lo = 4.0;
    r->species[2].ci_hi = 8.0;
    r->species[2].p_value = 0.001;
    r->species[2].read_pct = PARAM_LOW_WT_CEILING + 1.0;
    r->species[2].read_counts[0] = 15;  /* COI only */
    r->species[2].n_markers_detected = 1;

    /* sp3: 0 markers, weight below floor — EXCLUDED by weight/read_pct floor */
    strncpy(r->species[3].species_id, "Engraulis_encrasicolus", HS_MAX_NAME_LEN - 1);
    r->species[3].species_category = CATEGORY_REFERENCE;
    r->species[3].weight_pct = PARAM_WEIGHT_FLOOR - 0.5;
    r->species[3].ci_lo = 0.1;
    r->species[3].ci_hi = 1.0;
    r->species[3].p_value = 0.05;
    r->species[3].read_pct = PARAM_READ_PCT_FLOOR - 0.5;
    /* all read_counts are 0 (from calloc) */
    r->species[3].n_markers_detected = 0;

    return r;
}

static void test_marker_count_filter_json(void) {
    printf("  test_marker_count_filter_json...\n");
    halal_report_t *r = build_mock_report();

    /* Capture JSON output to a temp file */
    FILE *tmp = tmpfile();
    ASSERT(tmp != NULL, "tmpfile() created");
    report_print_json(r, tmp);

    /* Read back */
    fseek(tmp, 0, SEEK_END);
    long len = ftell(tmp);
    fseek(tmp, 0, SEEK_SET);
    char *buf = (char *)calloc(1, (size_t)len + 1);
    fread(buf, 1, (size_t)len, tmp);
    fclose(tmp);

    /* sp0: many markers, above ceiling -> INCLUDED */
    ASSERT(buf_contains(buf, "Bos_taurus"), "Bos_taurus included (many markers, above ceiling)");

    /* sp1: 0 markers, below ceiling -> EXCLUDED */
    ASSERT(!buf_contains(buf, "Dicentrarchus_labrax"), "D. labrax excluded (0 markers, below ceiling)");

    /* sp2: 0 markers, above ceiling -> INCLUDED */
    ASSERT(buf_contains(buf, "Sus_scrofa"), "Sus_scrofa included (above ceiling, immune to marker filter)");

    /* sp3: 0 markers, below floor -> EXCLUDED */
    ASSERT(!buf_contains(buf, "Engraulis_encrasicolus"), "Engraulis excluded (below weight floor)");

    free(buf);
    free(r);
}

static void test_marker_count_filter_tsv(void) {
    printf("  test_marker_count_filter_tsv...\n");
    halal_report_t *r = build_mock_report();

    FILE *tmp = tmpfile();
    ASSERT(tmp != NULL, "tmpfile() created");
    report_print_tsv(r, tmp);

    fseek(tmp, 0, SEEK_END);
    long len = ftell(tmp);
    fseek(tmp, 0, SEEK_SET);
    char *buf = (char *)calloc(1, (size_t)len + 1);
    fread(buf, 1, (size_t)len, tmp);
    fclose(tmp);

    ASSERT(buf_contains(buf, "Bos_taurus"), "TSV: Bos_taurus included (above ceiling)");
    ASSERT(!buf_contains(buf, "Dicentrarchus_labrax"), "TSV: D. labrax excluded (0 markers, below ceiling)");
    ASSERT(buf_contains(buf, "Sus_scrofa"), "TSV: Sus_scrofa included (above ceiling)");
    ASSERT(!buf_contains(buf, "Engraulis_encrasicolus"), "TSV: Engraulis excluded (below floor)");

    free(buf);
    free(r);
}

static void test_n_markers_in_json(void) {
    printf("  test_n_markers_in_json...\n");
    halal_report_t *r = build_mock_report();

    FILE *tmp = tmpfile();
    report_print_json(r, tmp);

    fseek(tmp, 0, SEEK_END);
    long len = ftell(tmp);
    fseek(tmp, 0, SEEK_SET);
    char *buf = (char *)calloc(1, (size_t)len + 1);
    fread(buf, 1, (size_t)len, tmp);
    fclose(tmp);

    /* Check that n_markers field appears in JSON for included species */
    ASSERT(buf_contains(buf, "\"n_markers\": 4"), "JSON contains n_markers: 4 for Bos_taurus");
    ASSERT(buf_contains(buf, "\"n_markers\": 1"), "JSON contains n_markers: 1 for Sus_scrofa");

    free(buf);
    free(r);
}

int main(void) {
    printf("=== Report Marker-Count Filter Tests ===\n");
    test_marker_count_filter_json();
    test_marker_count_filter_tsv();
    test_n_markers_in_json();
    printf("=== %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
