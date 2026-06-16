#ifndef SPECIESID_KMER_H
#define SPECIESID_KMER_H

#include <stdint.h>
#include "khash.h"

/* --- Hash table type for exact k-mer sets --- */
KHASH_SET_INIT_INT64(kmer64)

/* --- Core k-mer operations --- */
/* Encode base to 2-bit: A=0, C=1, G=2, T=3, other=-1 */
int hs_base_encode(char c);

/* Compute forward k-mer hash using MurmurHash3 finalizer */
uint64_t hs_kmer_hash(const char *seq, int k);

/* Get reverse complement hash */
uint64_t hs_kmer_revcomp_hash(const char *seq, int k);

/* Canonical hash: min(fwd, revcomp) */
uint64_t hs_kmer_canonical(const char *seq, int k);

/* Hash a pre-encoded 2-bit k-mer */
uint64_t hs_hash64(uint64_t key);

/* --- FracMinHash sketch (coarse screening, k=21) --- */
typedef struct {
    uint64_t *hashes;   /* sorted hash array */
    int n;              /* number of hashes stored */
    int cap;            /* capacity */
    double scale;       /* fraction of hash space to keep */
    int k;
    uint64_t threshold; /* = scale * UINT64_MAX */
} fmh_sketch_t;

fmh_sketch_t *fmh_init(int k, double scale);
void fmh_destroy(fmh_sketch_t *sk);
void fmh_add_hash(fmh_sketch_t *sk, uint64_t h);
void fmh_add_seq(fmh_sketch_t *sk, const char *seq, int len);
void fmh_sort(fmh_sketch_t *sk);
double fmh_containment(const fmh_sketch_t *query, const fmh_sketch_t *ref);
void fmh_merge(fmh_sketch_t *dst, const fmh_sketch_t *src);

/* --- Exact k-mer set (fine resolution, k=31) --- */
typedef struct {
    khash_t(kmer64) *h;
    int k;
    int n_kmers;
} kmer_set_t;

kmer_set_t *kmer_set_init(int k);
void kmer_set_destroy(kmer_set_t *ks);
void kmer_set_add_seq(kmer_set_t *ks, const char *seq, int len);
int kmer_set_contains(const kmer_set_t *ks, uint64_t h);
double kmer_set_containment(const char *query, int qlen, const kmer_set_t *ref, int k);

/* Remove all k-mers from 'ks' that appear in 'other'. Returns number removed. */
int kmer_set_subtract(kmer_set_t *ks, const kmer_set_t *other);

#endif /* SPECIESID_KMER_H */
