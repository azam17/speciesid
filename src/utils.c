#include "utils.h"
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>

/* --- Memory allocation --- */

void *hs_malloc(size_t size) {
    void *p = malloc(size);
    if (!p && size > 0) {
        fprintf(stderr, "[speciesid] ERROR: malloc(%zu) failed\n", size);
        abort();
    }
    return p;
}

void *hs_calloc(size_t n, size_t size) {
    void *p = calloc(n, size);
    if (!p && n > 0 && size > 0) {
        fprintf(stderr, "[speciesid] ERROR: calloc(%zu, %zu) failed\n", n, size);
        abort();
    }
    return p;
}

void *hs_realloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (!p && size > 0) {
        fprintf(stderr, "[speciesid] ERROR: realloc(%zu) failed\n", size);
        abort();
    }
    return p;
}

char *hs_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *d = (char *)hs_malloc(len);
    memcpy(d, s, len);
    return d;
}

/* --- Logging --- */

static hs_log_level_t g_log_level = HS_LOG_INFO;

void hs_log_set_level(hs_log_level_t level) {
    g_log_level = level;
}

void hs_log(hs_log_level_t level, const char *fmt, ...) {
    if (level < g_log_level) return;
    static const char *labels[] = { "DEBUG", "INFO", "WARN", "ERROR" };
    fprintf(stderr, "[speciesid][%s] ", labels[level]);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

/* --- Math helpers --- */

double hs_logsumexp(const double *arr, int n) {
    if (n <= 0) return -INFINITY;
    double mx = arr[0];
    for (int i = 1; i < n; i++)
        if (arr[i] > mx) mx = arr[i];
    if (mx == -INFINITY) return -INFINITY;
    double sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += exp(arr[i] - mx);
    return mx + log(sum);
}

double hs_log_add(double a, double b) {
    if (a == -INFINITY) return b;
    if (b == -INFINITY) return a;
    if (a > b) return a + log1p(exp(b - a));
    return b + log1p(exp(a - b));
}

void hs_normalize(double *arr, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; i++) sum += arr[i];
    if (sum > 0.0) {
        for (int i = 0; i < n; i++) arr[i] /= sum;
    }
}

void hs_log_normalize(double *log_arr, double *out, int n) {
    double lse = hs_logsumexp(log_arr, n);
    for (int i = 0; i < n; i++)
        out[i] = exp(log_arr[i] - lse);
}

/* --- RNG (xoshiro256**) --- */

static inline uint64_t rotl64(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

void hs_rng_seed(hs_rng_t *rng, uint64_t seed) {
    /* SplitMix64 to fill state */
    for (int i = 0; i < 4; i++) {
        seed += 0x9e3779b97f4a7c15ULL;
        uint64_t z = seed;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        rng->s[i] = z ^ (z >> 31);
    }
}

uint64_t hs_rng_next(hs_rng_t *rng) {
    uint64_t *s = rng->s;
    uint64_t result = rotl64(s[1] * 5, 7) * 9;
    uint64_t t = s[1] << 17;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl64(s[3], 45);
    return result;
}

double hs_rng_uniform(hs_rng_t *rng) {
    return (double)(hs_rng_next(rng) >> 11) * 0x1.0p-53;
}

double hs_rng_normal(hs_rng_t *rng) {
    /* Box-Muller transform */
    double u1 = hs_rng_uniform(rng);
    double u2 = hs_rng_uniform(rng);
    while (u1 < 1e-300) u1 = hs_rng_uniform(rng);
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

double hs_rng_lognormal(hs_rng_t *rng, double mu, double sigma) {
    return exp(mu + sigma * hs_rng_normal(rng));
}

void hs_rng_dirichlet(hs_rng_t *rng, const double *alpha, double *out, int n) {
    /* Generate gamma variates using Marsaglia & Tsang's method */
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        double a = alpha[i];
        double d, c, x, v, u;
        if (a < 1.0) {
            /* Boost: gamma(a) = gamma(a+1) * U^(1/a) */
            double boost = pow(hs_rng_uniform(rng), 1.0 / a);
            a += 1.0;
            d = a - 1.0 / 3.0;
            c = 1.0 / sqrt(9.0 * d);
            for (;;) {
                do { x = hs_rng_normal(rng); v = 1.0 + c * x; } while (v <= 0.0);
                v = v * v * v;
                u = hs_rng_uniform(rng);
                if (u < 1.0 - 0.0331 * (x * x) * (x * x)) { out[i] = d * v * boost; break; }
                if (log(u) < 0.5 * x * x + d * (1.0 - v + log(v))) { out[i] = d * v * boost; break; }
            }
        } else {
            d = a - 1.0 / 3.0;
            c = 1.0 / sqrt(9.0 * d);
            for (;;) {
                do { x = hs_rng_normal(rng); v = 1.0 + c * x; } while (v <= 0.0);
                v = v * v * v;
                u = hs_rng_uniform(rng);
                if (u < 1.0 - 0.0331 * (x * x) * (x * x)) { out[i] = d * v; break; }
                if (log(u) < 0.5 * x * x + d * (1.0 - v + log(v))) { out[i] = d * v; break; }
            }
        }
        if (out[i] < 1e-300) out[i] = 1e-300;
        sum += out[i];
    }
    for (int i = 0; i < n; i++) out[i] /= sum;
}

int hs_rng_categorical(hs_rng_t *rng, const double *probs, int n) {
    double u = hs_rng_uniform(rng);
    double cum = 0.0;
    for (int i = 0; i < n - 1; i++) {
        cum += probs[i];
        if (u < cum) return i;
    }
    return n - 1;
}

/* --- Misc --- */

int hs_file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

double hs_clock_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}
