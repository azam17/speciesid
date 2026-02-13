#include "report.h"
#include "utils.h"
#include <string.h>
#include <math.h>

const char *verdict_str(verdict_t v) {
    switch (v) {
        case PASS: return "PASS";
        case FAIL: return "FAIL";
        case INCONCLUSIVE: return "INCONCLUSIVE";
    }
    return "UNKNOWN";
}

halal_report_t *report_generate(const em_result_t *em,
                                 const halal_refdb_t *db,
                                 const read_result_t *classifications,
                                 int n_reads, double threshold) {
    halal_report_t *r = (halal_report_t *)hs_calloc(1, sizeof(halal_report_t));
    strncpy(r->sample_id, "sample", sizeof(r->sample_id) - 1);
    r->threshold_wpw = threshold;
    r->total_reads = n_reads;
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
    int n_haram_detected = 0;
    int n_mashbooh_detected = 0;
    for (int s = 0; s < r->n_species; s++) {
        strncpy(r->species[s].species_id, db->species[s].species_id, HS_MAX_NAME_LEN - 1);
        r->species[s].halal_status = db->species[s].status;
        r->species[s].weight_pct = em->w[s] * 100.0;
        r->species[s].ci_lo = em->w_ci_lo[s] * 100.0;
        r->species[s].ci_hi = em->w_ci_hi[s] * 100.0;
        r->species[s].p_value = (em->p_values) ? em->p_values[s] : 1.0;

        /* Raw read proportion */
        int total_hits = 0;
        for (int m = 0; m < HS_MAX_MARKERS; m++) total_hits += r->species[s].read_counts[m];
        r->species[s].read_pct = classified > 0 ? 100.0 * total_hits / classified : 0.0;

        /* Check halal status: requires BOTH weight >= threshold AND p < 0.05 */
        if (em->w[s] >= threshold && r->species[s].p_value < 0.05) {
            if (db->species[s].status == HARAM) n_haram_detected++;
            if (db->species[s].status == MASHBOOH) n_mashbooh_detected++;
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

    /* Verdict */
    if (n_haram_detected > 0) {
        r->verdict = FAIL;
    } else if (n_mashbooh_detected > 0 || agreement < 0.5 ||
               classified < n_reads / 10) {
        r->verdict = INCONCLUSIVE;
    } else {
        r->verdict = PASS;
    }

    return r;
}

void report_print_json(const halal_report_t *r, FILE *out) {
    fprintf(out, "{\n");
    fprintf(out, "  \"sample_id\": \"%s\",\n", r->sample_id);
    fprintf(out, "  \"verdict\": \"%s\",\n", verdict_str(r->verdict));
    fprintf(out, "  \"threshold_wpw\": %.4f,\n", r->threshold_wpw);
    fprintf(out, "  \"total_reads\": %d,\n", r->total_reads);
    fprintf(out, "  \"classified_reads\": %d,\n", r->classified_reads);
    fprintf(out, "  \"degradation_lambda\": %.6f,\n", r->degradation_lambda);
    fprintf(out, "  \"cross_marker_agreement\": %.4f,\n", r->cross_marker_agreement);
    fprintf(out, "  \"species\": [\n");
    int first = 1;
    for (int s = 0; s < r->n_species; s++) {
        const species_report_t *sp = &r->species[s];
        if (sp->weight_pct < 0.01 && sp->read_pct < 0.01) continue;
        if (!first) fprintf(out, ",\n");
        fprintf(out, "    {\n");
        fprintf(out, "      \"species\": \"%s\",\n", sp->species_id);
        fprintf(out, "      \"halal_status\": \"%s\",\n", halal_status_str(sp->halal_status));
        fprintf(out, "      \"weight_pct\": %.4f,\n", sp->weight_pct);
        fprintf(out, "      \"ci_lo\": %.4f,\n", sp->ci_lo);
        fprintf(out, "      \"ci_hi\": %.4f,\n", sp->ci_hi);
        fprintf(out, "      \"p_value\": %.4e,\n", sp->p_value);
        fprintf(out, "      \"read_pct\": %.4f\n", sp->read_pct);
        fprintf(out, "    }");
        first = 0;
    }
    fprintf(out, "\n  ]\n");
    fprintf(out, "}\n");
}

void report_print_tsv(const halal_report_t *r, FILE *out) {
    fprintf(out, "species\thalal_status\tweight_pct\tci_lo\tci_hi\tp_value\tread_pct\n");
    for (int s = 0; s < r->n_species; s++) {
        const species_report_t *sp = &r->species[s];
        if (sp->weight_pct < 0.01 && sp->read_pct < 0.01) continue;
        fprintf(out, "%s\t%s\t%.4f\t%.4f\t%.4f\t%.4e\t%.4f\n",
                sp->species_id, halal_status_str(sp->halal_status),
                sp->weight_pct, sp->ci_lo, sp->ci_hi, sp->p_value, sp->read_pct);
    }
}

void report_print_summary(const halal_report_t *r, FILE *out) {
    fprintf(out, "========================================\n");
    fprintf(out, "  SpeciesID Report: %s\n", r->sample_id);
    fprintf(out, "========================================\n");
    fprintf(out, "  Verdict: %s\n", verdict_str(r->verdict));
    fprintf(out, "  Reads: %d total, %d classified (%.1f%%)\n",
            r->total_reads, r->classified_reads,
            r->total_reads > 0 ? 100.0 * r->classified_reads / r->total_reads : 0.0);
    fprintf(out, "  Cross-marker agreement: %.1f%%\n", r->cross_marker_agreement * 100);
    fprintf(out, "------------------------------------------------------------\n");
    fprintf(out, "  %-20s %-10s %8s %8s %10s\n", "Species", "Status", "w/w%", "reads%", "p-value");
    fprintf(out, "  %-20s %-10s %8s %8s %10s\n", "-------", "------", "----", "------", "-------");
    for (int s = 0; s < r->n_species; s++) {
        const species_report_t *sp = &r->species[s];
        if (sp->weight_pct < 0.01 && sp->read_pct < 0.01) continue;
        char sig = (sp->p_value < 0.05) ? '*' : ' ';
        fprintf(out, "  %-20s %-10s %7.2f%%%c %7.2f%% %10.2e\n",
                sp->species_id, halal_status_str(sp->halal_status),
                sp->weight_pct, sig, sp->read_pct, sp->p_value);
    }
    if (r->classified_reads > 0)
        fprintf(out, "  (* indicates p < 0.05)\n");
    fprintf(out, "========================================\n");
}

void report_destroy(halal_report_t *r) {
    free(r);
}
