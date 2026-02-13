#include "kmer.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>

/* --- Base encoding --- */

static int base_table_init = 0;
static int base_table[256];

static void init_base_table(void) {
    if (base_table_init) return;
    for (int i = 0; i < 256; i++) base_table[i] = -1;
    base_table['A'] = 0; base_table['a'] = 0;
    base_table['C'] = 1; base_table['c'] = 1;
    base_table['G'] = 2; base_table['g'] = 2;
    base_table['T'] = 3; base_table['t'] = 3;
    base_table_init = 1;
}

int hs_base_encode(char c) {
    init_base_table();
    return base_table[(unsigned char)c];
}

/* --- MurmurHash3 64-bit finalizer --- */

uint64_t hs_hash64(uint64_t key) {
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53ULL;
    key ^= key >> 33;
    return key;
}

/* --- k-mer hashing --- */

uint64_t hs_kmer_hash(const char *seq, int k) {
    init_base_table();
    uint64_t kmer = 0;
    for (int i = 0; i < k; i++) {
        int b = base_table[(unsigned char)seq[i]];
        if (b < 0) return UINT64_MAX; /* invalid base */
        kmer = (kmer << 2) | (uint64_t)b;
    }
    return hs_hash64(kmer);
}

static const int comp_table[4] = { 3, 2, 1, 0 }; /* A<->T, C<->G */

uint64_t hs_kmer_revcomp_hash(const char *seq, int k) {
    uint64_t kmer = 0;
    for (int i = k - 1; i >= 0; i--) {
        int b = base_table[(unsigned char)seq[i]];
        if (b < 0) return UINT64_MAX;
        kmer = (kmer << 2) | (uint64_t)comp_table[b];
    }
    return hs_hash64(kmer);
}

uint64_t hs_kmer_canonical(const char *seq, int k) {
    uint64_t fwd = hs_kmer_hash(seq, k);
    uint64_t rev = hs_kmer_revcomp_hash(seq, k);
    if (fwd == UINT64_MAX || rev == UINT64_MAX) return UINT64_MAX;
    return fwd < rev ? fwd : rev;
}

/* --- FracMinHash sketch --- */

fmh_sketch_t *fmh_init(int k, double scale) {
    fmh_sketch_t *sk = (fmh_sketch_t *)hs_calloc(1, sizeof(fmh_sketch_t));
    sk->k = k;
    sk->scale = scale;
    sk->threshold = (uint64_t)(scale * (double)UINT64_MAX);
    sk->cap = 256;
    sk->hashes = (uint64_t *)hs_malloc(sk->cap * sizeof(uint64_t));
    sk->n = 0;
    return sk;
}

void fmh_destroy(fmh_sketch_t *sk) {
    if (sk) { free(sk->hashes); free(sk); }
}

void fmh_add_hash(fmh_sketch_t *sk, uint64_t h) {
    if (h > sk->threshold) return;
    if (sk->n >= sk->cap) {
        sk->cap *= 2;
        sk->hashes = (uint64_t *)hs_realloc(sk->hashes, sk->cap * sizeof(uint64_t));
    }
    sk->hashes[sk->n++] = h;
}

void fmh_add_seq(fmh_sketch_t *sk, const char *seq, int len) {
    if (len < sk->k) return;
    for (int i = 0; i <= len - sk->k; i++) {
        uint64_t h = hs_kmer_canonical(seq + i, sk->k);
        if (h != UINT64_MAX) fmh_add_hash(sk, h);
    }
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t va = *(const uint64_t *)a, vb = *(const uint64_t *)b;
    return (va > vb) - (va < vb);
}

void fmh_sort(fmh_sketch_t *sk) {
    qsort(sk->hashes, (size_t)sk->n, sizeof(uint64_t), cmp_u64);
    /* deduplicate */
    if (sk->n <= 1) return;
    int w = 1;
    for (int r = 1; r < sk->n; r++) {
        if (sk->hashes[r] != sk->hashes[r - 1])
            sk->hashes[w++] = sk->hashes[r];
    }
    sk->n = w;
}

double fmh_containment(const fmh_sketch_t *query, const fmh_sketch_t *ref) {
    if (query->n == 0) return 0.0;
    int shared = 0;
    int i = 0, j = 0;
    while (i < query->n && j < ref->n) {
        if (query->hashes[i] == ref->hashes[j]) { shared++; i++; j++; }
        else if (query->hashes[i] < ref->hashes[j]) i++;
        else j++;
    }
    return (double)shared / (double)query->n;
}

void fmh_merge(fmh_sketch_t *dst, const fmh_sketch_t *src) {
    for (int i = 0; i < src->n; i++)
        fmh_add_hash(dst, src->hashes[i]);
}

/* --- Exact k-mer set --- */

kmer_set_t *kmer_set_init(int k) {
    kmer_set_t *ks = (kmer_set_t *)hs_calloc(1, sizeof(kmer_set_t));
    ks->h = kh_init(kmer64);
    ks->k = k;
    ks->n_kmers = 0;
    return ks;
}

void kmer_set_destroy(kmer_set_t *ks) {
    if (ks) { kh_destroy(kmer64, ks->h); free(ks); }
}

void kmer_set_add_seq(kmer_set_t *ks, const char *seq, int len) {
    if (len < ks->k) return;
    for (int i = 0; i <= len - ks->k; i++) {
        uint64_t h = hs_kmer_canonical(seq + i, ks->k);
        if (h == UINT64_MAX) continue;
        int ret;
        kh_put(kmer64, ks->h, h, &ret);
        if (ret > 0) ks->n_kmers++;
    }
}

int kmer_set_contains(const kmer_set_t *ks, uint64_t h) {
    return kh_get(kmer64, ks->h, h) != kh_end(ks->h);
}

double kmer_set_containment(const char *query, int qlen, const kmer_set_t *ref, int k) {
    if (qlen < k) return 0.0;
    int total = 0, found = 0;
    for (int i = 0; i <= qlen - k; i++) {
        uint64_t h = hs_kmer_canonical(query + i, k);
        if (h == UINT64_MAX) continue;
        total++;
        if (kmer_set_contains(ref, h)) found++;
    }
    return total > 0 ? (double)found / (double)total : 0.0;
}
