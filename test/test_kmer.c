#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "kmer.h"
#include "utils.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, tol, msg) do { \
    if (fabs((a) - (b)) > (tol)) { \
        fprintf(stderr, "  FAIL: %s: %.6f != %.6f (line %d)\n", msg, (a), (b), __LINE__); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

static void test_base_encode(void) {
    printf("  test_base_encode...\n");
    ASSERT(hs_base_encode('A') == 0, "A encodes to 0");
    ASSERT(hs_base_encode('C') == 1, "C encodes to 1");
    ASSERT(hs_base_encode('G') == 2, "G encodes to 2");
    ASSERT(hs_base_encode('T') == 3, "T encodes to 3");
    ASSERT(hs_base_encode('a') == 0, "a encodes to 0");
    ASSERT(hs_base_encode('N') == -1, "N encodes to -1");
}

static void test_kmer_hash(void) {
    printf("  test_kmer_hash...\n");
    /* Same sequence should give same hash */
    uint64_t h1 = hs_kmer_hash("ACGT", 4);
    uint64_t h2 = hs_kmer_hash("ACGT", 4);
    ASSERT(h1 == h2, "Same sequence same hash");
    ASSERT(h1 != UINT64_MAX, "Valid hash");

    /* Different sequences should give different hashes (very likely) */
    uint64_t h3 = hs_kmer_hash("ACGA", 4);
    ASSERT(h1 != h3, "Different sequences different hashes");

    /* Invalid base returns UINT64_MAX */
    uint64_t h4 = hs_kmer_hash("ACGN", 4);
    ASSERT(h4 == UINT64_MAX, "Invalid base returns UINT64_MAX");
}

static void test_kmer_canonical(void) {
    printf("  test_kmer_canonical...\n");
    /* ACGT revcomp is ACGT, so canonical should be same as forward */
    uint64_t h1 = hs_kmer_canonical("ACGT", 4);
    ASSERT(h1 != UINT64_MAX, "Palindrome canonical hash valid");

    /* Canonical should be same for seq and its revcomp */
    /* AACG revcomp = CGTT */
    uint64_t h2 = hs_kmer_canonical("AACG", 4);
    uint64_t h3 = hs_kmer_canonical("CGTT", 4);
    ASSERT(h2 == h3, "Canonical hash symmetric for revcomp");
}

static void test_fmh_basic(void) {
    printf("  test_fmh_basic...\n");
    fmh_sketch_t *sk = fmh_init(4, 1.0); /* scale=1.0 keeps all */
    fmh_add_seq(sk, "ACGTACGT", 8);
    fmh_sort(sk);
    ASSERT(sk->n > 0, "FMH has hashes after adding seq");

    /* Self-containment should be 1.0 */
    double c = fmh_containment(sk, sk);
    ASSERT_NEAR(c, 1.0, 0.001, "Self-containment is 1.0");

    fmh_destroy(sk);
}

static void test_fmh_containment(void) {
    printf("  test_fmh_containment...\n");
    fmh_sketch_t *a = fmh_init(4, 1.0);
    fmh_sketch_t *b = fmh_init(4, 1.0);

    /* Build sketch from same sequence */
    const char *seq = "ACGTACGTACGTACGT";
    fmh_add_seq(a, seq, 16);
    fmh_add_seq(b, seq, 16);
    fmh_sort(a);
    fmh_sort(b);

    double c = fmh_containment(a, b);
    ASSERT_NEAR(c, 1.0, 0.001, "Same seq full containment");

    /* Different seq should have low containment */
    fmh_sketch_t *d = fmh_init(4, 1.0);
    fmh_add_seq(d, "TTTTTTTTTTTTTTTT", 16);
    fmh_sort(d);
    double c2 = fmh_containment(a, d);
    ASSERT(c2 < 0.5, "Different seq low containment");

    fmh_destroy(a);
    fmh_destroy(b);
    fmh_destroy(d);
}

static void test_fmh_scale(void) {
    printf("  test_fmh_scale...\n");
    /* With small scale, fewer hashes retained */
    fmh_sketch_t *full = fmh_init(4, 1.0);
    fmh_sketch_t *half = fmh_init(4, 0.5);

    const char *seq = "ACGTACGTACGTACGTACGTACGTACGTACGT";
    int len = (int)strlen(seq);
    fmh_add_seq(full, seq, len);
    fmh_add_seq(half, seq, len);
    fmh_sort(full);
    fmh_sort(half);

    ASSERT(half->n <= full->n, "Scaled sketch has fewer or equal hashes");

    fmh_destroy(full);
    fmh_destroy(half);
}

static void test_kmer_set(void) {
    printf("  test_kmer_set...\n");
    kmer_set_t *ks = kmer_set_init(4);
    kmer_set_add_seq(ks, "ACGTACGT", 8);
    ASSERT(ks->n_kmers > 0, "K-mer set has entries");

    /* Should contain the k-mers from the sequence */
    uint64_t h = hs_kmer_canonical("ACGT", 4);
    ASSERT(kmer_set_contains(ks, h), "Contains ACGT");

    kmer_set_destroy(ks);
}

static void test_kmer_set_containment(void) {
    printf("  test_kmer_set_containment...\n");
    kmer_set_t *ref = kmer_set_init(4);
    const char *refseq = "ACGTACGTACGTACGT";
    kmer_set_add_seq(ref, refseq, 16);

    /* Same sequence should have containment ~1.0 */
    double c1 = kmer_set_containment(refseq, 16, ref, 4);
    ASSERT_NEAR(c1, 1.0, 0.001, "Same seq containment 1.0");

    /* Substring should have high containment */
    double c2 = kmer_set_containment("ACGTACGT", 8, ref, 4);
    ASSERT(c2 > 0.8, "Substring high containment");

    /* Completely different should have low containment */
    double c3 = kmer_set_containment("TTTTTTTT", 8, ref, 4);
    ASSERT(c3 < 0.5, "Different seq low containment");

    kmer_set_destroy(ref);
}

static void test_fmh_merge(void) {
    printf("  test_fmh_merge...\n");
    fmh_sketch_t *a = fmh_init(4, 1.0);
    fmh_sketch_t *b = fmh_init(4, 1.0);
    fmh_add_seq(a, "ACGTACGT", 8);
    fmh_add_seq(b, "TGCATGCA", 8);
    fmh_sort(a);
    fmh_sort(b);
    int n_a = a->n;

    fmh_sketch_t *merged = fmh_init(4, 1.0);
    fmh_merge(merged, a);
    fmh_merge(merged, b);
    fmh_sort(merged);

    ASSERT(merged->n >= n_a, "Merged has at least as many hashes as a");

    fmh_destroy(a);
    fmh_destroy(b);
    fmh_destroy(merged);
}

int main(void) {
    printf("=== test_kmer ===\n");
    test_base_encode();
    test_kmer_hash();
    test_kmer_canonical();
    test_fmh_basic();
    test_fmh_containment();
    test_fmh_scale();
    test_kmer_set();
    test_kmer_set_containment();
    test_fmh_merge();
    printf("=== %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
