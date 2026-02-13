#ifndef SPECIESID_CALIBRATE_H
#define SPECIESID_CALIBRATE_H

/* Spike-in calibration: estimate prior parameters for d and b from
 * calibration samples with known composition.
 */

typedef struct {
    double *true_w;       /* [n_species] known weight fractions */
    double *obs_reads;    /* [n_species * n_markers] observed read counts */
    int n_species;
    int n_markers;
} calibration_sample_t;

typedef struct {
    double d_mu, d_sigma;    /* LogNormal prior on DNA yield */
    double b_mu, b_sigma;    /* LogNormal prior on PCR bias */
    int n_samples;
} calibration_result_t;

/* Estimate bias priors from spike-in calibration data */
calibration_result_t *calibrate_estimate(const calibration_sample_t *samples,
                                          int n_samples);

/* Save calibration result to file */
int calibrate_save(const calibration_result_t *result, const char *path);

/* Load calibration result from file */
calibration_result_t *calibrate_load(const char *path);

/* Load spike-in data from TSV file.
 * TSV format: species_id<tab>marker_id<tab>true_weight<tab>obs_reads
 * Returns array of calibration_sample_t, grouped by sample blocks.
 */
calibration_sample_t *calibrate_load_tsv(const char *path, int n_species,
                                          int n_markers, int *out_n_samples);

void calibrate_result_destroy(calibration_result_t *r);

#endif /* SPECIESID_CALIBRATE_H */
