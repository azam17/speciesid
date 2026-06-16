#ifndef SPECIESID_ANALYZE_H
#define SPECIESID_ANALYZE_H

#include <stddef.h>
#include "refdb.h"

#define ANALYZE_MAX_SAMPLES 64
#define ANALYZE_MAX_PATH    1024
#define ANALYZE_MAX_ID      256

typedef struct {
    char sample_id[ANALYZE_MAX_ID];
    char role[32];
    char fastq_path[ANALYZE_MAX_PATH];
    char label[ANALYZE_MAX_ID];
} analyze_sample_t;

typedef struct {
    char run_id[ANALYZE_MAX_ID];
    char index_path[ANALYZE_MAX_PATH];
    char database_hash[128];
    char calibration_profile_path[ANALYZE_MAX_PATH];
    char marker_ids[HS_MAX_MARKERS][16];
    int n_markers;
    analyze_sample_t samples[ANALYZE_MAX_SAMPLES];
    int n_samples;
    double detection_threshold;
    double prune_threshold;
    int n_bootstrap;
    char read_mode[16];
    int control_adjustment;
    int out_of_panel_check;
    int resolvability_check;
} analyze_manifest_t;

void analyze_manifest_init(analyze_manifest_t *m);
int analyze_manifest_load(const char *path, analyze_manifest_t *m,
                          char *err, size_t err_sz);
int analyze_execute_manifest(const char *manifest_path, const char *output_path);

#endif /* SPECIESID_ANALYZE_H */
