#ifndef SPECIESID_REFDB_H
#define SPECIESID_REFDB_H

#include <stdio.h>
#include <stdint.h>

#define HS_MAX_SPECIES  64
#define HS_MAX_MARKERS  8
#define HS_MAX_NAME_LEN 64
#define HS_MAX_PRIMER_LEN 64

typedef enum {
    CATEGORY_REFERENCE = 0,
    CATEGORY_EXCLUSION = 1,
    CATEGORY_REVIEW = 2,
    CATEGORY_UNKNOWN = 3
} species_category_t;

typedef struct {
    char species_id[HS_MAX_NAME_LEN];
    char common_name[HS_MAX_NAME_LEN];
    species_category_t category;
    double mito_copy_number;
    double dna_yield_prior;
} species_info_t;

typedef struct {
    int species_idx;
    int marker_idx;
    char *sequence;
    int seq_len;
    int amplicon_length;
} marker_ref_t;

typedef struct {
    species_info_t *species;
    int n_species;
    marker_ref_t *markers;   /* n_species * n_markers entries (row-major by species) */
    int n_markers;
    int n_marker_refs;       /* actual number of marker_ref entries */
    char marker_ids[HS_MAX_MARKERS][16];
    char primer_f[HS_MAX_MARKERS][HS_MAX_PRIMER_LEN];
    char primer_r[HS_MAX_MARKERS][HS_MAX_PRIMER_LEN];
    double threshold_wpw;
} halal_refdb_t;

/* Build reference database from a species TSV and FASTA directory */
halal_refdb_t *refdb_create(void);
int refdb_add_species(halal_refdb_t *db, const char *species_id,
                      const char *common_name, species_category_t category,
                      double mito_cn, double yield_prior);
int refdb_add_marker(halal_refdb_t *db, const char *marker_id,
                     const char *primer_f, const char *primer_r);
int refdb_add_marker_ref(halal_refdb_t *db, int species_idx, int marker_idx,
                         const char *sequence, int seq_len);

/* Serialization */
int refdb_save(const halal_refdb_t *db, const char *path);
halal_refdb_t *refdb_load(const char *path);
void refdb_destroy(halal_refdb_t *db);

/* Lookup helpers */
int refdb_find_species(const halal_refdb_t *db, const char *species_id);
int refdb_find_marker(const halal_refdb_t *db, const char *marker_id);
marker_ref_t *refdb_get_marker_ref(const halal_refdb_t *db, int species_idx, int marker_idx);
const char *species_category_str(species_category_t category);

/* Build a default database with built-in food authentication species */
halal_refdb_t *refdb_build_default(void);

/* Build database from a FASTA directory.
 * Expects files named: Species_name_MARKER.fa (e.g., Bos_taurus_COI.fa)
 * Species metadata (screening category, mito CN) uses built-in defaults. */
halal_refdb_t *refdb_build_from_fasta_dir(const char *fasta_dir);

#endif /* SPECIESID_REFDB_H */
