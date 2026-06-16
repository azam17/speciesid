#ifndef SPECIESID_REPORT_H
#define SPECIESID_REPORT_H

#include "refdb.h"
#include "em.h"
#include "classify.h"
#include <stdio.h>

typedef enum {
    SCREEN_CLEAR = 0,
    SCREEN_ALERT = 1,
    SCREEN_REVIEW = 2
} screening_result_t;

typedef struct {
    char species_id[HS_MAX_NAME_LEN];
    species_category_t species_category;
    double weight_pct;         /* w/w% */
    double ci_lo, ci_hi;       /* 95% CI */
    double p_value;            /* LRT p-value */
    double read_pct;           /* Raw read proportion */
    int read_counts[HS_MAX_MARKERS]; /* Per-marker read hit counts */
    int n_markers_detected;    /* Number of distinct markers with >0 reads */
    double detection_limit_wpw; /* Screening evidence threshold estimate at this read depth */
    int markers_positive[HS_MAX_MARKERS]; /* 1 if this marker had species hits */
    int markers_tested;        /* Total markers with >0 reads for this sample */
} species_report_t;

typedef struct {
    char sample_id[256];
    screening_result_t screening_result;
    species_report_t species[HS_MAX_SPECIES];
    int n_species;
    int total_reads;
    int classified_reads;
    double degradation_lambda;
    double cross_marker_agreement;
    double threshold_wpw;
    int bootstrap_stability_pct;  /* % of subsamples giving same species list, 0=not run */
    int n_bootstrap_resamples;    /* Number of bootstrap resamples, 0=not run */
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

const char *screening_result_str(screening_result_t result);

#endif /* SPECIESID_REPORT_H */
