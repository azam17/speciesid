#include "index.h"
#include "utils.h"
#include <string.h>

#define DEFAULT_COARSE_K  21
#define DEFAULT_FINE_K    21
#define DEFAULT_FMH_SCALE 0.01  /* 1% of hash space */

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

/* --- Serialization --- */

#define INDEX_MAGIC 0x48494458  /* "HIDX" */
#define INDEX_VERSION 2

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
    for (int m = 0; m < M; m++) {
        for (int s = 0; s < S; s++) {
            kmer_set_t *ks = idx->fine[m][s];
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

    fclose(fp);
    return 0;
}

halal_index_t *index_load(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    uint32_t magic, version;
    if (fread(&magic, 4, 1, fp) != 1 || magic != INDEX_MAGIC) { fclose(fp); return NULL; }
    if (fread(&version, 4, 1, fp) != 1 || version != INDEX_VERSION) { fclose(fp); return NULL; }

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
    idx->fine = (kmer_set_t ***)hs_calloc((size_t)M, sizeof(kmer_set_t **));
    for (int m = 0; m < M; m++) {
        idx->fine[m] = (kmer_set_t **)hs_calloc((size_t)S, sizeof(kmer_set_t *));
        for (int s = 0; s < S; s++) {
            int n, k;
            fread(&n, sizeof(int), 1, fp);
            fread(&k, sizeof(int), 1, fp);
            if (n > 0) {
                idx->fine[m][s] = kmer_set_init(k);
                for (int j = 0; j < n; j++) {
                    uint64_t key;
                    fread(&key, sizeof(uint64_t), 1, fp);
                    int ret;
                    kh_put(kmer64, idx->fine[m][s]->h, key, &ret);
                    idx->fine[m][s]->n_kmers++;
                }
            }
        }
    }

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

    return idx;
}
