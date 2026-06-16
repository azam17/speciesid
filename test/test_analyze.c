#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "analyze.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while (0)

static char *write_temp_manifest(const char *content) {
    char tmpl[] = "/tmp/speciesid_manifest_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return NULL;
    FILE *fp = fdopen(fd, "w");
    if (!fp) {
        close(fd);
        return NULL;
    }
    fputs(content, fp);
    fclose(fp);
    return strdup(tmpl);
}

static void test_valid_manifest_strings(void) {
    printf("  test_valid_manifest_strings...\n");
    const char *json =
        "{"
        "\"manifest_version\":\"1.0.0\","
        "\"run_id\":\"run-1\","
        "\"index_path\":\"/tmp/speciesid.idx\","
        "\"database_hash\":\"abc\","
        "\"calibration_profile_path\":null,"
        "\"marker_panel\":[\"16S\",\"12S\"],"
        "\"samples\":[{\"sample_id\":\"sample_1\",\"role\":\"sample\",\"fastq_path\":\"/tmp/a.fq\"}],"
        "\"analysis_params\":{\"detection_threshold\":0.002,\"control_adjustment\":false,\"read_mode\":\"short\"}"
        "}";
    char *path = write_temp_manifest(json);
    ASSERT(path != NULL, "temp manifest written");

    analyze_manifest_t m;
    char err[256] = {0};
    ASSERT(analyze_manifest_load(path, &m, err, sizeof(err)) == 0, "manifest parsed");
    ASSERT(strcmp(m.run_id, "run-1") == 0, "run_id parsed");
    ASSERT(m.n_markers == 2, "marker count parsed");
    ASSERT(strcmp(m.marker_ids[0], "16S") == 0, "first marker parsed");
    ASSERT(m.n_samples == 1, "sample count parsed");
    ASSERT(strcmp(m.samples[0].role, "sample") == 0, "sample role parsed");
    ASSERT(m.control_adjustment == 0, "control_adjustment parsed");
    ASSERT(strcmp(m.read_mode, "short") == 0, "read_mode parsed");

    unlink(path);
    free(path);
}

static void test_valid_manifest_marker_objects(void) {
    printf("  test_valid_manifest_marker_objects...\n");
    const char *json =
        "{"
        "\"run_id\":\"run-2\","
        "\"index_path\":\"/tmp/speciesid.idx\","
        "\"marker_panel\":[{\"marker_id\":\"COI\"}],"
        "\"samples\":[{\"sample_id\":\"sample_1\",\"role\":\"sample\",\"fastq_path\":\"/tmp/a.fq\"}]"
        "}";
    char *path = write_temp_manifest(json);
    analyze_manifest_t m;
    char err[256] = {0};
    ASSERT(analyze_manifest_load(path, &m, err, sizeof(err)) == 0,
           "object marker manifest parsed");
    ASSERT(strcmp(m.marker_ids[0], "COI") == 0, "object marker_id parsed");
    unlink(path);
    free(path);
}

static void test_invalid_marker_rejected(void) {
    printf("  test_invalid_marker_rejected...\n");
    const char *json =
        "{"
        "\"run_id\":\"run-3\","
        "\"index_path\":\"/tmp/speciesid.idx\","
        "\"marker_panel\":[\"BADMARKER\"],"
        "\"samples\":[{\"sample_id\":\"sample_1\",\"role\":\"sample\",\"fastq_path\":\"/tmp/a.fq\"}]"
        "}";
    char *path = write_temp_manifest(json);
    analyze_manifest_t m;
    char err[256] = {0};
    ASSERT(analyze_manifest_load(path, &m, err, sizeof(err)) != 0,
           "invalid marker rejected");
    ASSERT(strstr(err, "unknown marker") != NULL, "invalid marker error explained");
    unlink(path);
    free(path);
}

static void test_requires_test_sample(void) {
    printf("  test_requires_test_sample...\n");
    const char *json =
        "{"
        "\"run_id\":\"run-4\","
        "\"index_path\":\"/tmp/speciesid.idx\","
        "\"marker_panel\":[\"16S\"],"
        "\"samples\":[{\"sample_id\":\"ntc_1\",\"role\":\"negative_control\",\"fastq_path\":\"/tmp/a.fq\"}]"
        "}";
    char *path = write_temp_manifest(json);
    analyze_manifest_t m;
    char err[256] = {0};
    ASSERT(analyze_manifest_load(path, &m, err, sizeof(err)) != 0,
           "manifest without test sample rejected");
    ASSERT(strstr(err, "role=sample") != NULL, "missing test sample error explained");
    unlink(path);
    free(path);
}

int main(void) {
    printf("=== Analyze Manifest Tests ===\n");
    test_valid_manifest_strings();
    test_valid_manifest_marker_objects();
    test_invalid_marker_rejected();
    test_requires_test_sample();
    printf("=== %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
