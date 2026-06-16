#ifndef SPECIESID_CLASSIFY_H
#define SPECIESID_CLASSIFY_H

#include "index.h"

typedef struct {
    int species_idx;
    double containment;
} species_hit_t;

typedef struct {
    int marker_idx;           /* Which marker this read maps to (-1 = unknown) */
    species_hit_t *hits;      /* Candidate species + containment scores */
    int n_hits;
    int is_classified;
} read_result_t;

/* Read mode: auto-detect, force short (Illumina), or force long (Nanopore/PacBio) */
typedef enum {
    READ_MODE_AUTO  = 0,
    READ_MODE_SHORT = 1,
    READ_MODE_LONG  = 2
} read_mode_t;

typedef struct {
    double min_containment;   /* Minimum containment to report (Illumina: 0.3) */
    double coarse_threshold;  /* Coarse filter threshold (default: 0.05) */
    int is_nanopore;
    int n_threads;
    read_mode_t read_mode;    /* AUTO / SHORT / LONG */
    int lr_window_size;       /* Long-read window size (default: 300) */
    int lr_window_stride;     /* Long-read window stride (default: 150) */
} classify_opts_t;

classify_opts_t classify_opts_default(void);
classify_opts_t classify_opts_nanopore(void);
classify_opts_t classify_opts_longread(void);

/* Auto-detect read mode from a batch of read lengths */
read_mode_t classify_detect_read_mode(const int *lens, int n_reads);

/* Classify a batch of reads against the index */
read_result_t *classify_reads(const halal_index_t *idx,
                               const char **seqs, const int *lens, int n_reads,
                               const classify_opts_t *opts);

/* Free classification results */
void classify_results_free(read_result_t *results, int n);

/* Summary statistics */
typedef struct {
    int total_reads;
    int classified_reads;
    int per_marker[HS_MAX_MARKERS];
    int per_species[HS_MAX_SPECIES];
} classify_summary_t;

void classify_summarize(const read_result_t *results, int n,
                        const halal_index_t *idx, classify_summary_t *summary);

#endif /* SPECIESID_CLASSIFY_H */
