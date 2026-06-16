#include "index.h"
#include "params.h"
#include "utils.h"
#include <string.h>

#define DEFAULT_COARSE_K  PARAM_COARSE_K
#define DEFAULT_FINE_K    PARAM_FINE_K
#define DEFAULT_FMH_SCALE PARAM_FMH_SCALE

/* Long-read k-mer sizes (smaller for error tolerance) */
#define LR_COARSE_K  PARAM_LR_COARSE_K
#define LR_FINE_K    PARAM_LR_FINE_K
#define LR_PRIMER_K  PARAM_LR_PRIMER_K

/* Auto-scale for small sequences */
static double auto_scale(int min_seq_len, int k) {
    int n_kmers = min_seq_len - k + 1;
    if (n_kmers <= 0) return 1.0;
    /* Want at least 5 hashes per sequence */
    double needed = 5.0 / (double)n_kmers;
    if (needed > DEFAULT_FMH_SCALE) return needed < 1.0 ? needed : 1.0;
    return DEFAULT_FMH_SCALE;
}

halal_index_t *index_build(halal_refdb_t *db) {
    halal_index_t *idx = (halal_index_t *)hs_calloc(1, sizeof(halal_index_t));
    idx->db = db;
    idx->coarse_k = DEFAULT_COARSE_K;
    idx->fine_k = DEFAULT_FINE_K;

    int S = db->n_species;
    int M = db->n_markers;

    /* Find minimum sequence length for auto-scaling */
    int min_len = INT32_MAX;
    for (int i = 0; i < db->n_marker_refs; i++) {
        if (db->markers[i].seq_len < min_len)
            min_len = db->markers[i].seq_len;
    }
    idx->coarse_scale = auto_scale(min_len, idx->coarse_k);

    /* Build coarse sketches (one per species, merged across markers) */
    idx->coarse = (fmh_sketch_t **)hs_calloc((size_t)S, sizeof(fmh_sketch_t *));
    for (int s = 0; s < S; s++) {
        idx->coarse[s] = fmh_init(idx->coarse_k, idx->coarse_scale);
        for (int m = 0; m < M; m++) {
            marker_ref_t *mr = refdb_get_marker_ref(db, s, m);
            if (mr) fmh_add_seq(idx->coarse[s], mr->sequence, mr->seq_len);
        }
        fmh_sort(idx->coarse[s]);
    }

    /* Build fine k-mer sets (per marker, per species) */
    idx->fine = (kmer_set_t ***)hs_calloc((size_t)M, sizeof(kmer_set_t **));
    for (int m = 0; m < M; m++) {
        idx->fine[m] = (kmer_set_t **)hs_calloc((size_t)S, sizeof(kmer_set_t *));
        for (int s = 0; s < S; s++) {
            marker_ref_t *mr = refdb_get_marker_ref(db, s, m);
            if (mr) {
                /* Use smaller k if sequence is too short */
                int fk = idx->fine_k;
                if (mr->seq_len < fk) fk = mr->seq_len > 15 ? mr->seq_len : 15;
                idx->fine[m][s] = kmer_set_init(fk);
                kmer_set_add_seq(idx->fine[m][s], mr->sequence, mr->seq_len);
            }
        }
    }

    /* Build primer k-mer index for marker detection */
    idx->primer_index = (kmer_set_t **)hs_calloc((size_t)M, sizeof(kmer_set_t *));
    int primer_k = 15; /* use shorter k for primer matching */
    for (int m = 0; m < M; m++) {
        idx->primer_index[m] = kmer_set_init(primer_k);
        int flen = (int)strlen(db->primer_f[m]);
        int rlen = (int)strlen(db->primer_r[m]);
        if (flen >= primer_k)
            kmer_set_add_seq(idx->primer_index[m], db->primer_f[m], flen);
        if (rlen >= primer_k)
            kmer_set_add_seq(idx->primer_index[m], db->primer_r[m], rlen);
    }

    HS_LOG_INFO("Built index: %d species, %d markers, coarse_k=%d, fine_k=%d, scale=%.4f",
                S, M, idx->coarse_k, idx->fine_k, idx->coarse_scale);

    /* --- Build long-read sub-index at reduced k values --- */
    idx->lr_coarse_k = LR_COARSE_K;
    idx->lr_fine_k = LR_FINE_K;
    idx->lr_primer_k = LR_PRIMER_K;
    idx->lr_coarse_scale = auto_scale(min_len, LR_COARSE_K);
    idx->has_longread_index = 1;

    /* LR coarse sketches (k=11) */
    idx->lr_coarse = (fmh_sketch_t **)hs_calloc((size_t)S, sizeof(fmh_sketch_t *));
    for (int s = 0; s < S; s++) {
        idx->lr_coarse[s] = fmh_init(LR_COARSE_K, idx->lr_coarse_scale);
        for (int m = 0; m < M; m++) {
            marker_ref_t *mr = refdb_get_marker_ref(db, s, m);
            if (mr) fmh_add_seq(idx->lr_coarse[s], mr->sequence, mr->seq_len);
        }
        fmh_sort(idx->lr_coarse[s]);
    }

    /* LR fine k-mer sets (k=13) — build then subtract shared k-mers */
    idx->lr_fine = (kmer_set_t ***)hs_calloc((size_t)M, sizeof(kmer_set_t **));
    for (int m = 0; m < M; m++) {
        idx->lr_fine[m] = (kmer_set_t **)hs_calloc((size_t)S, sizeof(kmer_set_t *));
        for (int s = 0; s < S; s++) {
            marker_ref_t *mr = refdb_get_marker_ref(db, s, m);
            if (mr) {
                int fk = LR_FINE_K;
                if (mr->seq_len < fk) fk = mr->seq_len > 9 ? mr->seq_len : 9;
                idx->lr_fine[m][s] = kmer_set_init(fk);
                kmer_set_add_seq(idx->lr_fine[m][s], mr->sequence, mr->seq_len);
            }
        }
    }

    /* Remove shared k-mers: for each species pair, subtract k-mers that appear
     * in other species' reference for the same marker. This keeps only
     * species-unique k-mers for discriminative LR classification. */
    int total_removed = 0;
    for (int m = 0; m < M; m++) {
        for (int s = 0; s < S; s++) {
            if (!idx->lr_fine[m][s]) continue;
            for (int s2 = 0; s2 < S; s2++) {
                if (s2 == s || !idx->lr_fine[m][s2]) continue;
                total_removed += kmer_set_subtract(idx->lr_fine[m][s], idx->lr_fine[m][s2]);
            }
        }
    }
    HS_LOG_INFO("LR unique k-mers: removed %d shared k-mers across species", total_removed);

    /* LR primer index (k=9) */
    idx->lr_primer = (kmer_set_t **)hs_calloc((size_t)M, sizeof(kmer_set_t *));
    for (int m = 0; m < M; m++) {
        idx->lr_primer[m] = kmer_set_init(LR_PRIMER_K);
        int flen = (int)strlen(db->primer_f[m]);
        int rlen = (int)strlen(db->primer_r[m]);
        if (flen >= LR_PRIMER_K)
            kmer_set_add_seq(idx->lr_primer[m], db->primer_f[m], flen);
        if (rlen >= LR_PRIMER_K)
            kmer_set_add_seq(idx->lr_primer[m], db->primer_r[m], rlen);
    }

    HS_LOG_INFO("Built long-read sub-index: lr_coarse_k=%d, lr_fine_k=%d, lr_primer_k=%d, lr_scale=%.4f",
                idx->lr_coarse_k, idx->lr_fine_k, idx->lr_primer_k, idx->lr_coarse_scale);
    return idx;
}

void index_destroy(halal_index_t *idx) {
    if (!idx) return;
    int S = idx->db->n_species;
    int M = idx->db->n_markers;
    for (int s = 0; s < S; s++) fmh_destroy(idx->coarse[s]);
    free(idx->coarse);
    for (int m = 0; m < M; m++) {
        for (int s = 0; s < S; s++) kmer_set_destroy(idx->fine[m][s]);
        free(idx->fine[m]);
    }
    free(idx->fine);
    for (int m = 0; m < M; m++) kmer_set_destroy(idx->primer_index[m]);
    free(idx->primer_index);

    /* Free long-read sub-index */
    if (idx->has_longread_index) {
        for (int s = 0; s < S; s++) fmh_destroy(idx->lr_coarse[s]);
        free(idx->lr_coarse);
        for (int m = 0; m < M; m++) {
            for (int s = 0; s < S; s++) kmer_set_destroy(idx->lr_fine[m][s]);
            free(idx->lr_fine[m]);
        }
        free(idx->lr_fine);
        for (int m = 0; m < M; m++) kmer_set_destroy(idx->lr_primer[m]);
        free(idx->lr_primer);
    }

    refdb_destroy(idx->db);
    free(idx);
}

void index_query_coarse(const halal_index_t *idx, const char *seq, int len,
                        double *scores, int n_species) {
    fmh_sketch_t *qsk = fmh_init(idx->coarse_k, idx->coarse_scale);
    fmh_add_seq(qsk, seq, len);
    fmh_sort(qsk);
    for (int s = 0; s < n_species; s++)
        scores[s] = fmh_containment(qsk, idx->coarse[s]);
    fmh_destroy(qsk);
}

double index_query_fine(const halal_index_t *idx, const char *seq, int len,
                        int marker_idx, int species_idx) {
    kmer_set_t *ks = idx->fine[marker_idx][species_idx];
    if (!ks) return 0.0;
    return kmer_set_containment(seq, len, ks, ks->k);
}

int index_detect_marker(const halal_index_t *idx, const char *seq, int len) {
    int best_m = -1;
    double best_score = 0.0;
    for (int m = 0; m < idx->db->n_markers; m++) {
        kmer_set_t *pk = idx->primer_index[m];
        if (!pk || pk->n_kmers == 0) continue;
        double score = kmer_set_containment(seq, len, pk, pk->k);
        if (score > best_score) { best_score = score; best_m = m; }
    }
    /* Require at least some primer k-mer match */
    return best_score >= 0.1 ? best_m : -1;
}

/* --- Long-read query functions (use lr_* sub-index) --- */

void index_query_coarse_lr(const halal_index_t *idx, const char *seq, int len,
                           double *scores, int n_species) {
    fmh_sketch_t *qsk = fmh_init(idx->lr_coarse_k, idx->lr_coarse_scale);
    fmh_add_seq(qsk, seq, len);
    fmh_sort(qsk);
    for (int s = 0; s < n_species; s++)
        scores[s] = fmh_containment(qsk, idx->lr_coarse[s]);
    fmh_destroy(qsk);
}

double index_query_fine_lr(const halal_index_t *idx, const char *seq, int len,
                           int marker_idx, int species_idx) {
    kmer_set_t *ks = idx->lr_fine[marker_idx][species_idx];
    if (!ks) return 0.0;
    return kmer_set_containment(seq, len, ks, ks->k);
}

int index_detect_marker_lr(const halal_index_t *idx, const char *seq, int len) {
    int best_m = -1;
    double best_score = 0.0;
    for (int m = 0; m < idx->db->n_markers; m++) {
        kmer_set_t *pk = idx->lr_primer[m];
        if (!pk || pk->n_kmers == 0) continue;
        double score = kmer_set_containment(seq, len, pk, pk->k);
        if (score > best_score) { best_score = score; best_m = m; }
    }
    /* Lower threshold for noisy long reads */
    return best_score >= 0.05 ? best_m : -1;
}

/* --- Serialization --- */

#define INDEX_MAGIC 0x48494458  /* "HIDX" */
#define INDEX_VERSION 3

/* Helper: write a kmer_set_t array to file */
static void write_kmer_sets(FILE *fp, kmer_set_t ***sets, int n_outer, int n_inner) {
    for (int m = 0; m < n_outer; m++) {
        for (int s = 0; s < n_inner; s++) {
            kmer_set_t *ks = sets[m][s];
            int n = ks ? ks->n_kmers : 0;
            int k = ks ? ks->k : 0;
            fwrite(&n, sizeof(int), 1, fp);
            fwrite(&k, sizeof(int), 1, fp);
            if (ks && n > 0) {
                khint_t ki;
                for (ki = kh_begin(ks->h); ki != kh_end(ks->h); ki++) {
                    if (kh_exist(ks->h, ki)) {
                        uint64_t key = kh_key(ks->h, ki);
                        fwrite(&key, sizeof(uint64_t), 1, fp);
                    }
                }
            }
        }
    }
}

/* Helper: read kmer_set_t array from file */
static kmer_set_t ***read_kmer_sets(FILE *fp, int n_outer, int n_inner) {
    kmer_set_t ***sets = (kmer_set_t ***)hs_calloc((size_t)n_outer, sizeof(kmer_set_t **));
    for (int m = 0; m < n_outer; m++) {
        sets[m] = (kmer_set_t **)hs_calloc((size_t)n_inner, sizeof(kmer_set_t *));
        for (int s = 0; s < n_inner; s++) {
            int n, k;
            fread(&n, sizeof(int), 1, fp);
            fread(&k, sizeof(int), 1, fp);
            if (n > 0) {
                sets[m][s] = kmer_set_init(k);
                for (int j = 0; j < n; j++) {
                    uint64_t key;
                    fread(&key, sizeof(uint64_t), 1, fp);
                    int ret;
                    kh_put(kmer64, sets[m][s]->h, key, &ret);
                    sets[m][s]->n_kmers++;
                }
            }
        }
    }
    return sets;
}

int index_save(const halal_index_t *idx, const char *path) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    uint32_t magic = INDEX_MAGIC, version = INDEX_VERSION;
    fwrite(&magic, 4, 1, fp);
    fwrite(&version, 4, 1, fp);

    int S = idx->db->n_species;
    int M = idx->db->n_markers;
    fwrite(&idx->coarse_k, sizeof(int), 1, fp);
    fwrite(&idx->fine_k, sizeof(int), 1, fp);
    fwrite(&idx->coarse_scale, sizeof(double), 1, fp);

    /* Save refdb inline */
    /* Write species */
    fwrite(&S, sizeof(int), 1, fp);
    fwrite(&M, sizeof(int), 1, fp);
    fwrite(idx->db->species, sizeof(species_info_t), (size_t)S, fp);
    fwrite(idx->db->marker_ids, sizeof(idx->db->marker_ids), 1, fp);
    fwrite(idx->db->primer_f, sizeof(idx->db->primer_f), 1, fp);
    fwrite(idx->db->primer_r, sizeof(idx->db->primer_r), 1, fp);
    fwrite(&idx->db->threshold_wpw, sizeof(double), 1, fp);
    fwrite(&idx->db->n_marker_refs, sizeof(int), 1, fp);
    for (int i = 0; i < idx->db->n_marker_refs; i++) {
        fwrite(&idx->db->markers[i].species_idx, sizeof(int), 1, fp);
        fwrite(&idx->db->markers[i].marker_idx, sizeof(int), 1, fp);
        fwrite(&idx->db->markers[i].seq_len, sizeof(int), 1, fp);
        fwrite(&idx->db->markers[i].amplicon_length, sizeof(int), 1, fp);
        fwrite(idx->db->markers[i].sequence, 1, (size_t)idx->db->markers[i].seq_len, fp);
    }

    /* Save coarse sketches */
    for (int s = 0; s < S; s++) {
        fwrite(&idx->coarse[s]->n, sizeof(int), 1, fp);
        fwrite(idx->coarse[s]->hashes, sizeof(uint64_t), (size_t)idx->coarse[s]->n, fp);
    }

    /* Save fine k-mer sets */
    write_kmer_sets(fp, idx->fine, M, S);

    /* Save long-read sub-index */
    fwrite(&idx->has_longread_index, sizeof(int), 1, fp);
    if (idx->has_longread_index) {
        fwrite(&idx->lr_coarse_k, sizeof(int), 1, fp);
        fwrite(&idx->lr_fine_k, sizeof(int), 1, fp);
        fwrite(&idx->lr_primer_k, sizeof(int), 1, fp);
        fwrite(&idx->lr_coarse_scale, sizeof(double), 1, fp);

        /* LR coarse sketches */
        for (int s = 0; s < S; s++) {
            fwrite(&idx->lr_coarse[s]->n, sizeof(int), 1, fp);
            fwrite(idx->lr_coarse[s]->hashes, sizeof(uint64_t),
                   (size_t)idx->lr_coarse[s]->n, fp);
        }

        /* LR fine k-mer sets */
        write_kmer_sets(fp, idx->lr_fine, M, S);

        /* LR primer sets (stored as [1][M] for reuse of write_kmer_sets) */
        for (int m = 0; m < M; m++) {
            kmer_set_t *pk = idx->lr_primer[m];
            int n = pk ? pk->n_kmers : 0;
            int k = pk ? pk->k : 0;
            fwrite(&n, sizeof(int), 1, fp);
            fwrite(&k, sizeof(int), 1, fp);
            if (pk && n > 0) {
                khint_t ki;
                for (ki = kh_begin(pk->h); ki != kh_end(pk->h); ki++) {
                    if (kh_exist(pk->h, ki)) {
                        uint64_t key = kh_key(pk->h, ki);
                        fwrite(&key, sizeof(uint64_t), 1, fp);
                    }
                }
            }
        }
    }

    fclose(fp);
    return 0;
}

halal_index_t *index_load(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    uint32_t magic, version;
    if (fread(&magic, 4, 1, fp) != 1 || magic != INDEX_MAGIC) { fclose(fp); return NULL; }
    if (fread(&version, 4, 1, fp) != 1 || (version != 2 && version != INDEX_VERSION)) {
        fclose(fp); return NULL;
    }

    halal_index_t *idx = (halal_index_t *)hs_calloc(1, sizeof(halal_index_t));
    fread(&idx->coarse_k, sizeof(int), 1, fp);
    fread(&idx->fine_k, sizeof(int), 1, fp);
    fread(&idx->coarse_scale, sizeof(double), 1, fp);

    /* Load refdb */
    int S, M;
    fread(&S, sizeof(int), 1, fp);
    fread(&M, sizeof(int), 1, fp);

    halal_refdb_t *db = refdb_create();
    db->n_species = S;
    db->n_markers = M;
    db->species = (species_info_t *)hs_malloc((size_t)S * sizeof(species_info_t));
    fread(db->species, sizeof(species_info_t), (size_t)S, fp);
    fread(db->marker_ids, sizeof(db->marker_ids), 1, fp);
    fread(db->primer_f, sizeof(db->primer_f), 1, fp);
    fread(db->primer_r, sizeof(db->primer_r), 1, fp);
    fread(&db->threshold_wpw, sizeof(double), 1, fp);

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
    idx->db = db;

    /* Load coarse sketches */
    idx->coarse = (fmh_sketch_t **)hs_calloc((size_t)S, sizeof(fmh_sketch_t *));
    for (int s = 0; s < S; s++) {
        idx->coarse[s] = fmh_init(idx->coarse_k, idx->coarse_scale);
        fread(&idx->coarse[s]->n, sizeof(int), 1, fp);
        if (idx->coarse[s]->n > idx->coarse[s]->cap) {
            idx->coarse[s]->cap = idx->coarse[s]->n;
            idx->coarse[s]->hashes = (uint64_t *)hs_realloc(
                idx->coarse[s]->hashes, (size_t)idx->coarse[s]->n * sizeof(uint64_t));
        }
        fread(idx->coarse[s]->hashes, sizeof(uint64_t), (size_t)idx->coarse[s]->n, fp);
    }

    /* Load fine k-mer sets */
    idx->fine = read_kmer_sets(fp, M, S);

    /* Rebuild primer index from loaded primers */
    idx->primer_index = (kmer_set_t **)hs_calloc((size_t)M, sizeof(kmer_set_t *));
    int primer_k = 15;
    for (int m = 0; m < M; m++) {
        idx->primer_index[m] = kmer_set_init(primer_k);
        int flen = (int)strlen(db->primer_f[m]);
        int rlen = (int)strlen(db->primer_r[m]);
        if (flen >= primer_k)
            kmer_set_add_seq(idx->primer_index[m], db->primer_f[m], flen);
        if (rlen >= primer_k)
            kmer_set_add_seq(idx->primer_index[m], db->primer_r[m], rlen);
    }

    /* Load long-read sub-index (v3+) or skip (v2) */
    idx->has_longread_index = 0;
    if (version >= 3) {
        fread(&idx->has_longread_index, sizeof(int), 1, fp);
        if (idx->has_longread_index) {
            fread(&idx->lr_coarse_k, sizeof(int), 1, fp);
            fread(&idx->lr_fine_k, sizeof(int), 1, fp);
            fread(&idx->lr_primer_k, sizeof(int), 1, fp);
            fread(&idx->lr_coarse_scale, sizeof(double), 1, fp);

            /* LR coarse sketches */
            idx->lr_coarse = (fmh_sketch_t **)hs_calloc((size_t)S, sizeof(fmh_sketch_t *));
            for (int s = 0; s < S; s++) {
                idx->lr_coarse[s] = fmh_init(idx->lr_coarse_k, idx->lr_coarse_scale);
                fread(&idx->lr_coarse[s]->n, sizeof(int), 1, fp);
                if (idx->lr_coarse[s]->n > idx->lr_coarse[s]->cap) {
                    idx->lr_coarse[s]->cap = idx->lr_coarse[s]->n;
                    idx->lr_coarse[s]->hashes = (uint64_t *)hs_realloc(
                        idx->lr_coarse[s]->hashes,
                        (size_t)idx->lr_coarse[s]->n * sizeof(uint64_t));
                }
                fread(idx->lr_coarse[s]->hashes, sizeof(uint64_t),
                      (size_t)idx->lr_coarse[s]->n, fp);
            }

            /* LR fine k-mer sets */
            idx->lr_fine = read_kmer_sets(fp, M, S);

            /* LR primer sets */
            idx->lr_primer = (kmer_set_t **)hs_calloc((size_t)M, sizeof(kmer_set_t *));
            for (int m = 0; m < M; m++) {
                int n, k;
                fread(&n, sizeof(int), 1, fp);
                fread(&k, sizeof(int), 1, fp);
                if (n > 0) {
                    idx->lr_primer[m] = kmer_set_init(k);
                    for (int j = 0; j < n; j++) {
                        uint64_t key;
                        fread(&key, sizeof(uint64_t), 1, fp);
                        int ret;
                        kh_put(kmer64, idx->lr_primer[m]->h, key, &ret);
                        idx->lr_primer[m]->n_kmers++;
                    }
                } else {
                    idx->lr_primer[m] = kmer_set_init(k > 0 ? k : LR_PRIMER_K);
                }
            }
        }
    }

    return idx;
}
