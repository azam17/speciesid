#include "calibrate.h"
#include "utils.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

calibration_result_t *calibrate_estimate(const calibration_sample_t *samples,
                                          int n_samples) {
    if (n_samples <= 0) return NULL;

    calibration_result_t *res = (calibration_result_t *)hs_calloc(1, sizeof(*res));
    res->n_samples = n_samples;

    /* Estimate d and b from observed read proportions vs known composition.
     *
     * For each sample: obs[s,m] / total_m ~ w[s] * d[s] * b[s,m]
     * Given known w[s], estimate d[s] and b[s,m].
     */
    int S = samples[0].n_species;
    int M = samples[0].n_markers;

    /* Accumulate log(d) and log(b) across samples */
    double *log_d_sum = (double *)hs_calloc((size_t)S, sizeof(double));
    double *log_b_sum = (double *)hs_calloc((size_t)(S * M), sizeof(double));
    double *log_d_sq = (double *)hs_calloc((size_t)S, sizeof(double));
    double *log_b_sq = (double *)hs_calloc((size_t)(S * M), sizeof(double));
    int count = 0;

    for (int i = 0; i < n_samples; i++) {
        const calibration_sample_t *cs = &samples[i];
        /* Compute total reads per marker */
        double *marker_total = (double *)hs_calloc((size_t)M, sizeof(double));
        for (int s = 0; s < S; s++)
            for (int m = 0; m < M; m++)
                marker_total[m] += cs->obs_reads[s * M + m];

        for (int s = 0; s < S; s++) {
            if (cs->true_w[s] < 1e-10) continue;
            /* Estimate d * b_m for each marker */
            double *db = (double *)hs_malloc((size_t)M * sizeof(double));
            for (int m = 0; m < M; m++) {
                double obs_frac = marker_total[m] > 0 ?
                    cs->obs_reads[s * M + m] / marker_total[m] : 0;
                db[m] = obs_frac / cs->true_w[s];
                if (db[m] < 1e-10) db[m] = 1e-10;
            }
            /* d = geometric mean of db across markers */
            double log_d = 0;
            for (int m = 0; m < M; m++) log_d += log(db[m]);
            log_d /= M;
            log_d_sum[s] += log_d;
            log_d_sq[s] += log_d * log_d;
            /* b_m = db[m] / d */
            double d = exp(log_d);
            for (int m = 0; m < M; m++) {
                double bval = db[m] / d;
                double lb = log(bval);
                log_b_sum[s * M + m] += lb;
                log_b_sq[s * M + m] += lb * lb;
            }
            free(db);
        }
        free(marker_total);
        count++;
    }

    /* Compute mu and sigma from accumulated statistics */
    if (count > 0) {
        double d_mu_sum = 0, d_var_sum = 0;
        int d_count = 0;
        for (int s = 0; s < S; s++) {
            double mu = log_d_sum[s] / count;
            d_mu_sum += mu;
            d_count++;
            if (count > 1) {
                double var = log_d_sq[s] / count - mu * mu;
                d_var_sum += var > 0 ? var : 0.01;
            }
        }
        res->d_mu = d_count > 0 ? d_mu_sum / d_count : 0.0;
        res->d_sigma = count > 1 && d_count > 0 ?
            sqrt(d_var_sum / d_count) : 0.5;

        double b_mu_sum = 0, b_var_sum = 0;
        int b_count = 0;
        for (int s = 0; s < S; s++) {
            for (int m = 0; m < M; m++) {
                double mu = log_b_sum[s * M + m] / count;
                b_mu_sum += mu;
                b_count++;
                if (count > 1) {
                    double var = log_b_sq[s * M + m] / count - mu * mu;
                    b_var_sum += var > 0 ? var : 0.01;
                }
            }
        }
        res->b_mu = b_count > 0 ? b_mu_sum / b_count : 0.0;
        res->b_sigma = count > 1 && b_count > 0 ?
            sqrt(b_var_sum / b_count) : 0.5;
    }

    free(log_d_sum); free(log_b_sum);
    free(log_d_sq); free(log_b_sq);

    if (res->d_sigma < 0.1) res->d_sigma = 0.1;
    if (res->b_sigma < 0.1) res->b_sigma = 0.1;

    return res;
}

int calibrate_save(const calibration_result_t *result, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    fprintf(fp, "d_mu=%.10f\n", result->d_mu);
    fprintf(fp, "d_sigma=%.10f\n", result->d_sigma);
    fprintf(fp, "b_mu=%.10f\n", result->b_mu);
    fprintf(fp, "b_sigma=%.10f\n", result->b_sigma);
    fprintf(fp, "n_samples=%d\n", result->n_samples);
    fclose(fp);
    return 0;
}

calibration_result_t *calibrate_load(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;

    calibration_result_t *r = (calibration_result_t *)hs_calloc(1, sizeof(*r));
    r->d_mu = 0.0; r->d_sigma = 0.5;
    r->b_mu = 0.0; r->b_sigma = 0.5;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "d_mu=%lf", &r->d_mu) == 1) continue;
        if (sscanf(line, "d_sigma=%lf", &r->d_sigma) == 1) continue;
        if (sscanf(line, "b_mu=%lf", &r->b_mu) == 1) continue;
        if (sscanf(line, "b_sigma=%lf", &r->b_sigma) == 1) continue;
        if (sscanf(line, "n_samples=%d", &r->n_samples) == 1) continue;
    }
    fclose(fp);
    return r;
}

calibration_sample_t *calibrate_load_tsv(const char *path, int n_species,
                                          int n_markers, int *out_n_samples) {
    /* TSV format per block:
     * #sample<tab>sample_name
     * species_idx<tab>marker_idx<tab>true_weight<tab>obs_reads
     * ... (n_species * n_markers lines per sample)
     *
     * Simplified: flat file with columns:
     * sample_id<tab>species_idx<tab>marker_idx<tab>true_weight<tab>obs_reads
     */
    FILE *fp = fopen(path, "r");
    if (!fp) { *out_n_samples = 0; return NULL; }

    /* First pass: count samples */
    int max_samples = 128;
    calibration_sample_t *samples = (calibration_sample_t *)hs_calloc(
        (size_t)max_samples, sizeof(calibration_sample_t));
    int n_samples = 0;
    int cur_sample = -1;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        int sample_id, si, mi;
        double tw, obs;
        if (sscanf(line, "%d\t%d\t%d\t%lf\t%lf", &sample_id, &si, &mi, &tw, &obs) != 5)
            continue;

        if (sample_id != cur_sample) {
            cur_sample = sample_id;
            if (n_samples >= max_samples) break;
            samples[n_samples].n_species = n_species;
            samples[n_samples].n_markers = n_markers;
            samples[n_samples].true_w = (double *)hs_calloc(
                (size_t)n_species, sizeof(double));
            samples[n_samples].obs_reads = (double *)hs_calloc(
                (size_t)(n_species * n_markers), sizeof(double));
            n_samples++;
        }

        int idx = n_samples - 1;
        if (si >= 0 && si < n_species) {
            samples[idx].true_w[si] = tw;
            if (mi >= 0 && mi < n_markers)
                samples[idx].obs_reads[si * n_markers + mi] = obs;
        }
    }
    fclose(fp);

    *out_n_samples = n_samples;
    return samples;
}

void calibrate_result_destroy(calibration_result_t *r) {
    free(r);
}
