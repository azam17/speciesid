#ifndef SPECIESID_SIMULATE_H
#define SPECIESID_SIMULATE_H

#include "refdb.h"
#include <stdint.h>

typedef struct {
    double *composition;    /* [n_species] true w/w fractions */
    int n_species;
    double *pcr_bias;       /* [S*M] true bias values (NULL = no bias) */
    double *dna_yield;      /* [S] true yield factors (NULL = all 1.0) */
    double degradation;     /* lambda_proc */
    int reads_per_marker;
    double error_rate;      /* 0.001 Illumina, 0.05 Nanopore */
    int read_length;
    uint64_t seed;
} sim_config_t;

typedef struct {
    char **reads;
    int *read_lengths;
    int *true_species;
    int *true_marker;
    int n_reads;
    sim_config_t config;
} sim_result_t;

sim_result_t *simulate_mixture(const sim_config_t *config, const halal_refdb_t *db);
void sim_result_destroy(sim_result_t *sr);

/* Write simulated reads to a FASTQ file */
int sim_write_fastq(const sim_result_t *sr, const char *path);

#endif /* SPECIESID_SIMULATE_H */
