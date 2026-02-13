#include "simulate.h"
#include "utils.h"
#include <string.h>
#include <math.h>

/* Mutate a sequence with given error rate */
static void mutate_seq(char *seq, int len, double error_rate, hs_rng_t *rng) {
    static const char bases[] = "ACGT";
    for (int i = 0; i < len; i++) {
        if (hs_rng_uniform(rng) < error_rate) {
            char orig = seq[i];
            do { seq[i] = bases[hs_rng_next(rng) & 3]; } while (seq[i] == orig);
        }
    }
}

/* Extract a random substring (simulating a read) */
static char *extract_read(const char *ref, int ref_len, int read_len, hs_rng_t *rng) {
    if (ref_len <= read_len) {
        return hs_strdup(ref);
    }
    int start = (int)(hs_rng_uniform(rng) * (ref_len - read_len));
    char *read = (char *)hs_malloc((size_t)read_len + 1);
    memcpy(read, ref + start, (size_t)read_len);
    read[read_len] = '\0';
    return read;
}

sim_result_t *simulate_mixture(const sim_config_t *config, const halal_refdb_t *db) {
    hs_rng_t rng;
    hs_rng_seed(&rng, config->seed);

    int S = config->n_species < db->n_species ? config->n_species : db->n_species;
    int M = db->n_markers;
    int rpm = config->reads_per_marker > 0 ? config->reads_per_marker : 1000;
    int total_reads = rpm * M;

    sim_result_t *sr = (sim_result_t *)hs_calloc(1, sizeof(sim_result_t));
    sr->reads = (char **)hs_malloc((size_t)total_reads * sizeof(char *));
    sr->read_lengths = (int *)hs_malloc((size_t)total_reads * sizeof(int));
    sr->true_species = (int *)hs_malloc((size_t)total_reads * sizeof(int));
    sr->true_marker = (int *)hs_malloc((size_t)total_reads * sizeof(int));
    sr->n_reads = 0;

    /* Copy config */
    sr->config = *config;
    sr->config.composition = (double *)hs_malloc((size_t)S * sizeof(double));
    memcpy(sr->config.composition, config->composition, (size_t)S * sizeof(double));

    /* Compute effective probabilities: p_sm ~ w[s] * d[s] * b[s,m] */
    double *p = (double *)hs_calloc((size_t)(S * M), sizeof(double));
    for (int m = 0; m < M; m++) {
        double sum = 0.0;
        for (int s = 0; s < S; s++) {
            double d = (config->dna_yield && s < config->n_species) ?
                       config->dna_yield[s] : 1.0;
            double b = (config->pcr_bias) ? config->pcr_bias[s * M + m] : 1.0;
            /* Check if this species has a reference for this marker */
            marker_ref_t *mr = refdb_get_marker_ref(db, s, m);
            if (!mr) { p[s * M + m] = 0.0; continue; }
            p[s * M + m] = config->composition[s] * d * b;
            sum += p[s * M + m];
        }
        /* Normalize per marker */
        if (sum > 0)
            for (int s = 0; s < S; s++) p[s * M + m] /= sum;
    }

    /* Generate reads */
    for (int m = 0; m < M; m++) {
        /* Build CDF for this marker */
        double *cdf = (double *)hs_calloc((size_t)S, sizeof(double));
        cdf[0] = p[0 * M + m];
        for (int s = 1; s < S; s++)
            cdf[s] = cdf[s - 1] + p[s * M + m];

        for (int r = 0; r < rpm; r++) {
            /* Pick species */
            double u = hs_rng_uniform(&rng);
            int sp = S - 1;
            for (int s = 0; s < S; s++) {
                if (u <= cdf[s]) { sp = s; break; }
            }

            /* Get reference sequence */
            marker_ref_t *mr = refdb_get_marker_ref(db, sp, m);
            if (!mr) continue; /* skip if no ref */

            int rlen = config->read_length > 0 ? config->read_length : mr->seq_len;
            char *read = extract_read(mr->sequence, mr->seq_len, rlen, &rng);
            int actual_len = (int)strlen(read);

            /* Apply sequencing errors */
            mutate_seq(read, actual_len, config->error_rate, &rng);

            int idx = sr->n_reads;
            sr->reads[idx] = read;
            sr->read_lengths[idx] = actual_len;
            sr->true_species[idx] = sp;
            sr->true_marker[idx] = m;
            sr->n_reads++;
        }
        free(cdf);
    }

    free(p);
    return sr;
}

void sim_result_destroy(sim_result_t *sr) {
    if (!sr) return;
    for (int i = 0; i < sr->n_reads; i++) free(sr->reads[i]);
    free(sr->reads);
    free(sr->read_lengths);
    free(sr->true_species);
    free(sr->true_marker);
    free(sr->config.composition);
    free(sr);
}

int sim_write_fastq(const sim_result_t *sr, const char *path) {
    FILE *fp;
    if (strcmp(path, "-") == 0) {
        fp = stdout;
    } else {
        fp = fopen(path, "w");
        if (!fp) return -1;
    }

    for (int i = 0; i < sr->n_reads; i++) {
        fprintf(fp, "@read_%d species=%d marker=%d\n", i,
                sr->true_species[i], sr->true_marker[i]);
        fprintf(fp, "%s\n", sr->reads[i]);
        fprintf(fp, "+\n");
        /* Fake quality scores */
        for (int j = 0; j < sr->read_lengths[i]; j++) fputc('I', fp);
        fputc('\n', fp);
    }

    if (fp != stdout) fclose(fp);
    return 0;
}
