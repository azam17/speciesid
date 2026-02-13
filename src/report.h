#ifndef SPECIESID_REPORT_H
#define SPECIESID_REPORT_H

#include "refdb.h"
#include "em.h"
#include "classify.h"
#include <stdio.h>

typedef enum { PASS = 0, FAIL = 1, INCONCLUSIVE = 2 } verdict_t;

typedef struct {
    char species_id[HS_MAX_NAME_LEN];
    halal_status_t halal_status;
    double weight_pct;         /* w/w% */
    double ci_lo, ci_hi;       /* 95% CI */
    double p_value;            /* LRT p-value */
    double read_pct;           /* Raw read proportion */
    int read_counts[HS_MAX_MARKERS]; /* Per-marker read counts */
} species_report_t;

typedef struct {
    char sample_id[256];
    verdict_t verdict;
    species_report_t species[HS_MAX_SPECIES];
    int n_species;
    int total_reads;
    int classified_reads;
    double degradation_lambda;
    double cross_marker_agreement;
    double threshold_wpw;
} halal_report_t;

/* Generate report from EM results */
halal_report_t *report_generate(const em_result_t *em,
                                 const halal_refdb_t *db,
                                 const read_result_t *classifications,
                                 int n_reads, double threshold);

/* Output formats */
void report_print_json(const halal_report_t *r, FILE *out);
void report_print_tsv(const halal_report_t *r, FILE *out);
void report_print_summary(const halal_report_t *r, FILE *out);

void report_destroy(halal_report_t *r);

const char *verdict_str(verdict_t v);

#endif /* SPECIESID_REPORT_H */
