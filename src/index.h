#ifndef SPECIESID_INDEX_H
#define SPECIESID_INDEX_H

#include "kmer.h"
#include "refdb.h"

typedef struct {
    /* Coarse level: one FracMinHash per species (merged across markers) */
    fmh_sketch_t **coarse;         /* [n_species] */
    /* Fine level: per-marker per-species exact k-mer sets */
    kmer_set_t ***fine;            /* [n_markers][n_species], NULL if no ref */
    /* Marker detection: primer k-mer sets for each marker */
    kmer_set_t **primer_index;     /* [n_markers] */
    halal_refdb_t *db;             /* reference (owned) */
    int coarse_k;                  /* 21 */
    int fine_k;                    /* 31 */
    double coarse_scale;           /* FracMinHash scale */
} halal_index_t;

halal_index_t *index_build(halal_refdb_t *db);
int index_save(const halal_index_t *idx, const char *path);
halal_index_t *index_load(const char *path);
void index_destroy(halal_index_t *idx);

/* Query: get coarse containment for a read against all species */
void index_query_coarse(const halal_index_t *idx, const char *seq, int len,
                        double *scores, int n_species);

/* Query: get fine containment for read vs species at a specific marker */
double index_query_fine(const halal_index_t *idx, const char *seq, int len,
                        int marker_idx, int species_idx);

/* Detect marker from read using primer k-mer matching */
int index_detect_marker(const halal_index_t *idx, const char *seq, int len);

#endif /* SPECIESID_INDEX_H */
