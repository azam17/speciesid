#include "report.h"
#include "params.h"
#include "utils.h"
#include <string.h>
#include <math.h>

const char *screening_result_str(screening_result_t result) {
    switch (result) {
        case SCREEN_CLEAR:  return "CLEAR";
        case SCREEN_ALERT:  return "ALERT";
        case SCREEN_REVIEW: return "REVIEW";
    }
    return "UNKNOWN";
}

static void copy_cstr(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return;
    size_t i = 0;
    if (src) {
        for (; i + 1 < dst_sz && src[i] != '\0'; i++) dst[i] = src[i];
    }
    dst[i] = '\0';
}

halal_report_t *report_generate(const em_result_t *em,
                                 const halal_refdb_t *db,
                                 const read_result_t *classifications,
                                 int n_reads, double threshold) {
    halal_report_t *r = (halal_report_t *)hs_calloc(1, sizeof(halal_report_t));
    copy_cstr(r->sample_id, sizeof(r->sample_id), "sample");
    r->threshold_wpw = threshold;
    r->total_reads = n_reads;
    r->bootstrap_stability_pct = 0;
    r->n_bootstrap_resamples = 0;

    /* Handle case where EM returned NULL (0 classified reads) */
    if (!em) {
        r->degradation_lambda = 0.0;
        r->n_species = 0;
        r->classified_reads = 0;
        r->cross_marker_agreement = 0.0;
        r->screening_result = SCREEN_REVIEW;
        return r;
    }

    r->degradation_lambda = em->lambda_proc;
    r->n_species = em->n_species < HS_MAX_SPECIES ? em->n_species : HS_MAX_SPECIES;

    /* Count classified reads and per-species/marker counts */
    int classified = 0;
    for (int i = 0; i < n_reads; i++) {
        if (!classifications[i].is_classified) continue;
        classified++;
        for (int j = 0; j < classifications[i].n_hits; j++) {
            int s = classifications[i].hits[j].species_idx;
            int m = classifications[i].marker_idx;
            if (s >= 0 && s < r->n_species && m >= 0 && m < HS_MAX_MARKERS)
                r->species[s].read_counts[m]++;
        }
    }
    r->classified_reads = classified;

    /* Fill species info */
    int n_exclusion_detected = 0;
    int n_review_detected = 0;
    int markers_seen[HS_MAX_MARKERS] = {0};
    int n_markers_tested = 0;

    /* Count which markers have any reads in this sample */
    for (int i = 0; i < n_reads; i++) {
        if (!classifications[i].is_classified) continue;
        int m = classifications[i].marker_idx;
        if (m >= 0 && m < HS_MAX_MARKERS) markers_seen[m] = 1;
    }
    for (int m = 0; m < HS_MAX_MARKERS; m++)
        if (markers_seen[m]) n_markers_tested++;

    for (int s = 0; s < r->n_species; s++) {
        copy_cstr(r->species[s].species_id, sizeof(r->species[s].species_id),
                  db->species[s].species_id);
        r->species[s].species_category = db->species[s].category;
        r->species[s].weight_pct = em->w[s] * 100.0;
        r->species[s].ci_lo = em->w_ci_lo[s] * 100.0;
        r->species[s].ci_hi = em->w_ci_hi[s] * 100.0;
        r->species[s].p_value = (em->p_values) ? em->p_values[s] : 1.0;

        /* Raw read proportion and marker count */
        int total_hits = 0;
        int n_markers_hit = 0;
        for (int m = 0; m < HS_MAX_MARKERS; m++) {
            total_hits += r->species[s].read_counts[m];
            if (r->species[s].read_counts[m] > 0) {
                n_markers_hit++;
                r->species[s].markers_positive[m] = 1;
            } else {
                r->species[s].markers_positive[m] = 0;
            }
        }
        r->species[s].read_pct = classified > 0 ? 100.0 * total_hits / classified : 0.0;
        r->species[s].n_markers_detected = n_markers_hit;
        r->species[s].markers_tested = n_markers_tested;

        /* Evidence threshold estimate at this read depth.
         * Uses 3 / sqrt(N_classified) heuristic — the approximate lower bound
         * on detectable proportion given binomial sampling. More reads = lower
         * screening evidence threshold estimate. Scaled by 100 for percentage output. */
        r->species[s].detection_limit_wpw = classified > 0
            ? 300.0 / sqrt((double)classified)
            : 100.0; /* no reads = can't detect anything */

        /* Flag configured categories only when evidence clears threshold and LRT. */
        if (em->w[s] >= threshold && r->species[s].p_value < 0.05) {
            if (db->species[s].category == CATEGORY_EXCLUSION) n_exclusion_detected++;
            if (db->species[s].category == CATEGORY_REVIEW) n_review_detected++;
        }
    }

    /* Cross-marker agreement: measure consistency of species ranking across markers */
    double agreement = 1.0;
    if (db->n_markers >= 2) {
        int agree = 0, total_pairs = 0;
        for (int m1 = 0; m1 < db->n_markers; m1++) {
            for (int m2 = m1 + 1; m2 < db->n_markers; m2++) {
                /* Check if top species is consistent */
                int top1 = -1, top2 = -1;
                int max1 = 0, max2 = 0;
                for (int s = 0; s < r->n_species; s++) {
                    if (r->species[s].read_counts[m1] > max1) {
                        max1 = r->species[s].read_counts[m1]; top1 = s;
                    }
                    if (r->species[s].read_counts[m2] > max2) {
                        max2 = r->species[s].read_counts[m2]; top2 = s;
                    }
                }
                if (top1 >= 0 && top2 >= 0) {
                    if (top1 == top2) agree++;
                    total_pairs++;
                }
            }
        }
        agreement = total_pairs > 0 ? (double)agree / total_pairs : 0.0;
    }
    r->cross_marker_agreement = agreement;

    /* Screening result; this is analytical evidence, not certification. */
    if (n_exclusion_detected > 0) {
        r->screening_result = SCREEN_ALERT;
    } else if (n_review_detected > 0 || agreement < 0.5 ||
               classified < n_reads / 10) {
        r->screening_result = SCREEN_REVIEW;
    } else {
        r->screening_result = SCREEN_CLEAR;
    }

    return r;
}

void report_print_json(const halal_report_t *r, FILE *out) {
    fprintf(out, "{\n");
    fprintf(out, "  \"sample_id\": \"%s\",\n", r->sample_id);
    fprintf(out, "  \"screening_result\": \"%s\",\n", screening_result_str(r->screening_result));
    fprintf(out, "  \"threshold_wpw\": %.4f,\n", r->threshold_wpw);
    fprintf(out, "  \"total_reads\": %d,\n", r->total_reads);
    fprintf(out, "  \"classified_reads\": %d,\n", r->classified_reads);
    fprintf(out, "  \"degradation_lambda\": %.6f,\n", r->degradation_lambda);
    fprintf(out, "  \"cross_marker_agreement\": %.4f,\n", r->cross_marker_agreement);
    if (r->n_bootstrap_resamples > 0) {
        fprintf(out, "  \"bootstrap_stability_pct\": %d,\n", r->bootstrap_stability_pct);
        fprintf(out, "  \"n_bootstrap_resamples\": %d,\n", r->n_bootstrap_resamples);
    }
    fprintf(out, "  \"species\": [\n");
    int first = 1;
    for (int s = 0; s < r->n_species; s++) {
        const species_report_t *sp = &r->species[s];
        if (sp->weight_pct < PARAM_WEIGHT_FLOOR && sp->read_pct < PARAM_READ_PCT_FLOOR) continue;
        if (sp->n_markers_detected < PARAM_MIN_MARKERS_FOR_LOW_WT && sp->weight_pct < PARAM_LOW_WT_CEILING) continue;
        if (!first) fprintf(out, ",\n");
        fprintf(out, "    {\n");
        fprintf(out, "      \"species\": \"%s\",\n", sp->species_id);
        fprintf(out, "      \"species_category\": \"%s\",\n", species_category_str(sp->species_category));
        fprintf(out, "      \"weight_pct\": %.4f,\n", sp->weight_pct);
        fprintf(out, "      \"ci_lo\": %.4f,\n", sp->ci_lo);
        fprintf(out, "      \"ci_hi\": %.4f,\n", sp->ci_hi);
        fprintf(out, "      \"p_value\": %.4e,\n", sp->p_value);
        fprintf(out, "      \"read_pct\": %.4f,\n", sp->read_pct);
        fprintf(out, "      \"n_markers\": %d,\n", sp->n_markers_detected);
        fprintf(out, "      \"markers_tested\": %d,\n", sp->markers_tested);
        fprintf(out, "      \"evidence_threshold_estimate_wpw\": %.4f,\n", sp->detection_limit_wpw);
        fprintf(out, "      \"marker_evidence\": {");
        int mfirst = 1;
        for (int m = 0; m < HS_MAX_MARKERS; m++) {
            if (sp->read_counts[m] > 0) {
                fprintf(out, "%s\"m%d\": %d", mfirst ? "" : ", ", m, sp->read_counts[m]);
                mfirst = 0;
            }
        }
        fprintf(out, "}\n");
        fprintf(out, "    }");
        first = 0;
    }
    fprintf(out, "\n  ]\n");
    fprintf(out, "}\n");
}

void report_print_tsv(const halal_report_t *r, FILE *out) {
    fprintf(out, "species\tspecies_category\tweight_pct\tci_lo\tci_hi\tp_value\tread_pct\tn_markers\tevidence_threshold_estimate_wpw\n");
    for (int s = 0; s < r->n_species; s++) {
        const species_report_t *sp = &r->species[s];
        if (sp->weight_pct < PARAM_WEIGHT_FLOOR && sp->read_pct < PARAM_READ_PCT_FLOOR) continue;
        if (sp->n_markers_detected < PARAM_MIN_MARKERS_FOR_LOW_WT && sp->weight_pct < PARAM_LOW_WT_CEILING) continue;
        fprintf(out, "%s\t%s\t%.4f\t%.4f\t%.4f\t%.4e\t%.4f\t%d\t%.4f\n",
                sp->species_id, species_category_str(sp->species_category),
                sp->weight_pct, sp->ci_lo, sp->ci_hi, sp->p_value, sp->read_pct,
                sp->n_markers_detected, sp->detection_limit_wpw);
    }
}

void report_print_summary(const halal_report_t *r, FILE *out) {
    fprintf(out, "========================================\n");
    fprintf(out, "  SpeciesID Report: %s\n", r->sample_id);
    fprintf(out, "========================================\n");
    fprintf(out, "  Screening result: %s\n", screening_result_str(r->screening_result));
    fprintf(out, "  Reads: %d total, %d classified (%.1f%%)\n",
            r->total_reads, r->classified_reads,
            r->total_reads > 0 ? 100.0 * r->classified_reads / r->total_reads : 0.0);
    fprintf(out, "  Cross-marker agreement: %.1f%%\n", r->cross_marker_agreement * 100);
    if (r->n_bootstrap_resamples > 0)
        fprintf(out, "  Bootstrap stability: %d%% (%d resamples)\n",
                r->bootstrap_stability_pct, r->n_bootstrap_resamples);
    fprintf(out, "  Evidence threshold estimate: %.2f%% w/w (at %d classified reads)\n",
            r->classified_reads > 0 ? 300.0 / sqrt((double)r->classified_reads) : 100.0,
            r->classified_reads);
    fprintf(out, "------------------------------------------------------------\n");
    fprintf(out, "  %-20s %-10s %8s %8s %10s %s\n", "Species", "Category", "w/w%", "reads%", "p-value", "markers");
    fprintf(out, "  %-20s %-10s %8s %8s %10s %s\n", "-------", "--------", "----", "------", "-------", "-------");
    for (int s = 0; s < r->n_species; s++) {
        const species_report_t *sp = &r->species[s];
        if (sp->weight_pct < PARAM_WEIGHT_FLOOR && sp->read_pct < PARAM_READ_PCT_FLOOR) continue;
        if (sp->n_markers_detected < PARAM_MIN_MARKERS_FOR_LOW_WT && sp->weight_pct < PARAM_LOW_WT_CEILING) continue;
        char sig = (sp->p_value < 0.05) ? '*' : ' ';
        fprintf(out, "  %-20s %-10s %7.2f%%%c %7.2f%% %10.2e %d\n",
                sp->species_id, species_category_str(sp->species_category),
                sp->weight_pct, sig, sp->read_pct, sp->p_value,
                sp->n_markers_detected);
    }
    if (r->classified_reads > 0)
        fprintf(out, "  (* indicates p < 0.05)\n");
    fprintf(out, "========================================\n");
}

void report_destroy(halal_report_t *r) {
    free(r);
}
