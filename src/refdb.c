#include "refdb.h"
#include "refseqs.h"
#include "utils.h"
#include <string.h>
#include <errno.h>

/* Magic + version for serialization */
#define REFDB_MAGIC  0x48414C41  /* "HALA" */
#define REFDB_VERSION 1

const char *halal_status_str(halal_status_t s) {
    switch (s) {
        case HALAL:     return "HALAL";
        case HARAM:     return "HARAM";
        case MASHBOOH:  return "MASHBOOH";
        default:        return "UNKNOWN";
    }
}

halal_refdb_t *refdb_create(void) {
    halal_refdb_t *db = (halal_refdb_t *)hs_calloc(1, sizeof(halal_refdb_t));
    db->threshold_wpw = 0.001; /* 0.1% w/w default */
    return db;
}

int refdb_add_species(halal_refdb_t *db, const char *species_id,
                      const char *common_name, halal_status_t status,
                      double mito_cn, double yield_prior) {
    if (db->n_species >= HS_MAX_SPECIES) return -1;
    int idx = db->n_species;
    db->species = (species_info_t *)hs_realloc(db->species,
        (size_t)(idx + 1) * sizeof(species_info_t));
    memset(&db->species[idx], 0, sizeof(species_info_t));
    strncpy(db->species[idx].species_id, species_id, HS_MAX_NAME_LEN - 1);
    strncpy(db->species[idx].common_name, common_name, HS_MAX_NAME_LEN - 1);
    db->species[idx].status = status;
    db->species[idx].mito_copy_number = mito_cn;
    db->species[idx].dna_yield_prior = yield_prior;
    db->n_species++;
    return idx;
}

int refdb_add_marker(halal_refdb_t *db, const char *marker_id,
                     const char *primer_f, const char *primer_r) {
    if (db->n_markers >= HS_MAX_MARKERS) return -1;
    int idx = db->n_markers;
    strncpy(db->marker_ids[idx], marker_id, 15);
    if (primer_f) strncpy(db->primer_f[idx], primer_f, HS_MAX_PRIMER_LEN - 1);
    if (primer_r) strncpy(db->primer_r[idx], primer_r, HS_MAX_PRIMER_LEN - 1);
    db->n_markers++;
    return idx;
}

int refdb_add_marker_ref(halal_refdb_t *db, int species_idx, int marker_idx,
                         const char *sequence, int seq_len) {
    if (species_idx < 0 || species_idx >= db->n_species) return -1;
    if (marker_idx < 0 || marker_idx >= db->n_markers) return -1;
    int idx = db->n_marker_refs;
    db->markers = (marker_ref_t *)hs_realloc(db->markers,
        (size_t)(idx + 1) * sizeof(marker_ref_t));
    db->markers[idx].species_idx = species_idx;
    db->markers[idx].marker_idx = marker_idx;
    db->markers[idx].sequence = hs_strdup(sequence);
    db->markers[idx].seq_len = seq_len;
    db->markers[idx].amplicon_length = seq_len;
    db->n_marker_refs++;
    return idx;
}

int refdb_find_species(const halal_refdb_t *db, const char *species_id) {
    for (int i = 0; i < db->n_species; i++)
        if (strcmp(db->species[i].species_id, species_id) == 0) return i;
    return -1;
}

int refdb_find_marker(const halal_refdb_t *db, const char *marker_id) {
    for (int i = 0; i < db->n_markers; i++)
        if (strcmp(db->marker_ids[i], marker_id) == 0) return i;
    return -1;
}

marker_ref_t *refdb_get_marker_ref(const halal_refdb_t *db, int species_idx, int marker_idx) {
    for (int i = 0; i < db->n_marker_refs; i++) {
        if (db->markers[i].species_idx == species_idx &&
            db->markers[i].marker_idx == marker_idx)
            return &db->markers[i];
    }
    return NULL;
}

/* --- Serialization --- */

int refdb_save(const halal_refdb_t *db, const char *path) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    uint32_t magic = REFDB_MAGIC, version = REFDB_VERSION;
    fwrite(&magic, 4, 1, fp);
    fwrite(&version, 4, 1, fp);

    /* Species */
    fwrite(&db->n_species, sizeof(int), 1, fp);
    fwrite(db->species, sizeof(species_info_t), (size_t)db->n_species, fp);

    /* Markers metadata */
    fwrite(&db->n_markers, sizeof(int), 1, fp);
    fwrite(db->marker_ids, sizeof(db->marker_ids), 1, fp);
    fwrite(db->primer_f, sizeof(db->primer_f), 1, fp);
    fwrite(db->primer_r, sizeof(db->primer_r), 1, fp);

    /* Marker references */
    fwrite(&db->n_marker_refs, sizeof(int), 1, fp);
    for (int i = 0; i < db->n_marker_refs; i++) {
        fwrite(&db->markers[i].species_idx, sizeof(int), 1, fp);
        fwrite(&db->markers[i].marker_idx, sizeof(int), 1, fp);
        fwrite(&db->markers[i].seq_len, sizeof(int), 1, fp);
        fwrite(&db->markers[i].amplicon_length, sizeof(int), 1, fp);
        fwrite(db->markers[i].sequence, 1, (size_t)db->markers[i].seq_len, fp);
    }

    fwrite(&db->threshold_wpw, sizeof(double), 1, fp);
    fclose(fp);
    return 0;
}

halal_refdb_t *refdb_load(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    uint32_t magic, version;
    if (fread(&magic, 4, 1, fp) != 1 || magic != REFDB_MAGIC) { fclose(fp); return NULL; }
    if (fread(&version, 4, 1, fp) != 1 || version != REFDB_VERSION) { fclose(fp); return NULL; }

    halal_refdb_t *db = refdb_create();

    fread(&db->n_species, sizeof(int), 1, fp);
    db->species = (species_info_t *)hs_malloc((size_t)db->n_species * sizeof(species_info_t));
    fread(db->species, sizeof(species_info_t), (size_t)db->n_species, fp);

    fread(&db->n_markers, sizeof(int), 1, fp);
    fread(db->marker_ids, sizeof(db->marker_ids), 1, fp);
    fread(db->primer_f, sizeof(db->primer_f), 1, fp);
    fread(db->primer_r, sizeof(db->primer_r), 1, fp);

    fread(&db->n_marker_refs, sizeof(int), 1, fp);
    db->markers = (marker_ref_t *)hs_malloc((size_t)db->n_marker_refs * sizeof(marker_ref_t));
    for (int i = 0; i < db->n_marker_refs; i++) {
        fread(&db->markers[i].species_idx, sizeof(int), 1, fp);
        fread(&db->markers[i].marker_idx, sizeof(int), 1, fp);
        fread(&db->markers[i].seq_len, sizeof(int), 1, fp);
        fread(&db->markers[i].amplicon_length, sizeof(int), 1, fp);
        db->markers[i].sequence = (char *)hs_malloc((size_t)db->markers[i].seq_len + 1);
        fread(db->markers[i].sequence, 1, (size_t)db->markers[i].seq_len, fp);
        db->markers[i].sequence[db->markers[i].seq_len] = '\0';
    }

    fread(&db->threshold_wpw, sizeof(double), 1, fp);
    fclose(fp);
    return db;
}

void refdb_destroy(halal_refdb_t *db) {
    if (!db) return;
    free(db->species);
    for (int i = 0; i < db->n_marker_refs; i++)
        free(db->markers[i].sequence);
    free(db->markers);
    free(db);
}

/* --- Build default database with real NCBI mitochondrial sequences --- */

halal_refdb_t *refdb_build_default(void) {
    halal_refdb_t *db = refdb_create();

    /* Add markers: COI, cytb, 16S with primer sequences */
    refdb_add_marker(db, "COI",
        "GGTCAACAAATCATAAAGATATTGG",     /* LCO1490 forward primer */
        "TAAACTTCAGGGTGACCAAAAAATCA");   /* HCO2198 reverse primer */
    refdb_add_marker(db, "cytb",
        "CCATCCAACATCTCAGCATGATG",       /* L14724 forward primer */
        "CCCCTCAGAATGATATTTGTCCTCA");    /* H15149 reverse primer */
    refdb_add_marker(db, "16S",
        "GACGAGAAGACCCTATGGAGC",         /* Tillmar 2013 forward primer */
        "TCCGAGGTCGCCCCAACC");           /* Tillmar 2013 reverse primer */

    struct { const char *id; const char *name; halal_status_t status; double mito_cn; double yield; } sp[] = {
        { "Sus_scrofa",           "Domestic pig",     HARAM,    1800, 1.0  },
        { "Sus_barbatus",         "Bearded pig",      HARAM,    1750, 0.95 },
        { "Canis_lupus",          "Dog",              HARAM,    1200, 0.8  },
        { "Bos_taurus",           "Cattle",           HALAL,    2000, 1.2  },
        { "Bubalus_bubalis",      "Water buffalo",    HALAL,    1900, 1.1  },
        { "Ovis_aries",           "Sheep",            HALAL,    1700, 1.0  },
        { "Capra_hircus",         "Goat",             HALAL,    1600, 1.0  },
        { "Gallus_gallus",        "Chicken",          HALAL,    1000, 0.7  },
        { "Meleagris_gallopavo",  "Turkey",           HALAL,     900, 0.6  },
        { "Anas_platyrhynchos",   "Duck",             HALAL,     950, 0.65 },
        { "Equus_caballus",       "Horse",            MASHBOOH, 1500, 0.9  },
        { "Equus_asinus",         "Donkey",           MASHBOOH, 1400, 0.85 },
        { "Salmo_salar",          "Atlantic salmon",  HALAL,    1200, 0.7  },
        { "Oreochromis_niloticus", "Nile tilapia",    HALAL,    1100, 0.65 },
        { "Gadus_morhua",         "Atlantic cod",     HALAL,    1300, 0.75 },
        { "Pangasianodon_hypophthalmus", "Pangasius", HALAL,   1250, 0.7  },
        { "Glycine_max",          "Soybean",          HALAL,     100, 0.5  },
        { "Triticum_aestivum",    "Bread wheat",      HALAL,      80, 0.45 },
        { "Oryza_sativa",         "Rice",             HALAL,      90, 0.48 },
    };
    int n_sp = (int)(sizeof(sp) / sizeof(sp[0]));

    for (int i = 0; i < n_sp; i++) {
        int si = refdb_add_species(db, sp[i].id, sp[i].name, sp[i].status,
                                   sp[i].mito_cn, sp[i].yield);
        /* Add real NCBI mitochondrial amplicon sequences from refseqs.h */
        for (int m = 0; m < 3; m++) {
            const refseq_entry_t *entry = &REFSEQ_TABLE[i][m];
            if (entry->seq && entry->len > 0) {
                refdb_add_marker_ref(db, si, m, entry->seq, entry->len);
            }
        }
    }

    return db;
}

/* --- Build database from a FASTA directory --- */

/* Read a single FASTA sequence, concatenating all lines after header */
static char *read_fasta_seq(const char *path, int *out_len) {
    FILE *fp = fopen(path, "r");
    if (!fp) { *out_len = 0; return NULL; }
    char line[4096];
    int cap = 4096, len = 0;
    char *seq = (char *)hs_malloc((size_t)cap);
    while (fgets(line, (int)sizeof(line), fp)) {
        if (line[0] == '>') continue;
        int ll = (int)strlen(line);
        while (ll > 0 && (line[ll-1] == '\n' || line[ll-1] == '\r')) ll--;
        if (len + ll >= cap) { cap *= 2; seq = (char *)hs_realloc(seq, (size_t)cap); }
        memcpy(seq + len, line, (size_t)ll);
        len += ll;
    }
    seq[len] = '\0';
    fclose(fp);
    *out_len = len;
    return seq;
}

/* Known species metadata table */
static const struct {
    const char *id;
    const char *name;
    halal_status_t status;
    double mito_cn;
    double yield;
} known_species[] = {
    { "Sus_scrofa",          "Domestic pig",     HARAM,    1800, 1.0 },
    { "Sus_barbatus",        "Bearded pig",      HARAM,    1750, 0.95 },
    { "Canis_lupus",         "Dog",              HARAM,    1200, 0.8 },
    { "Bos_taurus",          "Cattle",           HALAL,    2000, 1.2 },
    { "Bubalus_bubalis",     "Water buffalo",    HALAL,    1900, 1.1 },
    { "Ovis_aries",          "Sheep",            HALAL,    1700, 1.0 },
    { "Capra_hircus",        "Goat",             HALAL,    1600, 1.0 },
    { "Gallus_gallus",       "Chicken",          HALAL,    1000, 0.7 },
    { "Meleagris_gallopavo", "Turkey",           HALAL,     900, 0.6 },
    { "Anas_platyrhynchos",  "Duck",             HALAL,     950, 0.65 },
    { "Equus_caballus",      "Horse",            MASHBOOH, 1500, 0.9  },
    { "Equus_asinus",        "Donkey",           MASHBOOH, 1400, 0.85 },
    { "Salmo_salar",         "Atlantic salmon",  HALAL,    1200, 0.7  },
    { "Oreochromis_niloticus", "Nile tilapia",    HALAL,    1100, 0.65 },
    { "Gadus_morhua",        "Atlantic cod",     HALAL,    1300, 0.75 },
    { "Pangasianodon_hypophthalmus", "Pangasius", HALAL,   1250, 0.7  },
    { "Glycine_max",         "Soybean",          HALAL,     100, 0.5  },
    { "Triticum_aestivum",   "Bread wheat",      HALAL,      80, 0.45 },
    { "Oryza_sativa",        "Rice",             HALAL,      90, 0.48 },
    { NULL, NULL, 0, 0, 0 }
};

halal_refdb_t *refdb_build_from_fasta_dir(const char *fasta_dir) {
    halal_refdb_t *db = refdb_create();

    /* Add markers */
    refdb_add_marker(db, "COI",
        "GGTCAACAAATCATAAAGATATTGG",
        "TAAACTTCAGGGTGACCAAAAAATCA");
    refdb_add_marker(db, "cytb",
        "CCATCCAACATCTCAGCATGATG",
        "CCCCTCAGAATGATATTTGTCCTCA");
    refdb_add_marker(db, "16S",
        "GACGAGAAGACCCTATGGAGC",         /* Tillmar 2013 forward */
        "TCCGAGGTCGCCCCAACC");           /* Tillmar 2013 reverse */

    const char *marker_names[] = { "COI", "cytb", "16S" };
    int n_markers = 3;

    /* Scan for known species with FASTA files in directory */
    for (int ki = 0; known_species[ki].id != NULL; ki++) {
        const char *sp_id = known_species[ki].id;
        int has_any = 0;
        for (int m = 0; m < n_markers; m++) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s_%s.fa", fasta_dir, sp_id, marker_names[m]);
            if (hs_file_exists(path)) { has_any = 1; break; }
        }
        if (!has_any) continue;

        int si = refdb_add_species(db, sp_id, known_species[ki].name,
                                    known_species[ki].status,
                                    known_species[ki].mito_cn,
                                    known_species[ki].yield);
        for (int m = 0; m < n_markers; m++) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s_%s.fa", fasta_dir, sp_id, marker_names[m]);
            int seq_len = 0;
            char *seq = read_fasta_seq(path, &seq_len);
            if (seq && seq_len > 0) {
                refdb_add_marker_ref(db, si, m, seq, seq_len);
                HS_LOG_INFO("  %s %s: %d bp", sp_id, marker_names[m], seq_len);
            }
            free(seq);
        }
    }

    HS_LOG_INFO("Built database from %s: %d species, %d markers, %d refs",
                fasta_dir, db->n_species, db->n_markers, db->n_marker_refs);
    return db;
}
