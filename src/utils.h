#ifndef SPECIESID_UTILS_H
#define SPECIESID_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* --- Memory allocation with error checking --- */
void *hs_malloc(size_t size);
void *hs_calloc(size_t n, size_t size);
void *hs_realloc(void *ptr, size_t size);
char *hs_strdup(const char *s);

/* --- Logging --- */
typedef enum { HS_LOG_DEBUG = 0, HS_LOG_INFO, HS_LOG_WARN, HS_LOG_ERROR } hs_log_level_t;

void hs_log_set_level(hs_log_level_t level);
void hs_log(hs_log_level_t level, const char *fmt, ...);

#define HS_LOG_DEBUG(...) hs_log(HS_LOG_DEBUG, __VA_ARGS__)
#define HS_LOG_INFO(...)  hs_log(HS_LOG_INFO, __VA_ARGS__)
#define HS_LOG_WARN(...)  hs_log(HS_LOG_WARN, __VA_ARGS__)
#define HS_LOG_ERROR(...) hs_log(HS_LOG_ERROR, __VA_ARGS__)

/* --- Math helpers --- */
double hs_logsumexp(const double *arr, int n);
double hs_log_add(double a, double b);
void hs_normalize(double *arr, int n);
void hs_log_normalize(double *log_arr, double *out, int n);

/* --- Simple RNG (xoshiro256**) --- */
typedef struct {
    uint64_t s[4];
} hs_rng_t;

void hs_rng_seed(hs_rng_t *rng, uint64_t seed);
uint64_t hs_rng_next(hs_rng_t *rng);
double hs_rng_uniform(hs_rng_t *rng);       /* [0, 1) */
double hs_rng_normal(hs_rng_t *rng);        /* standard normal */
double hs_rng_lognormal(hs_rng_t *rng, double mu, double sigma);
void hs_rng_dirichlet(hs_rng_t *rng, const double *alpha, double *out, int n);
int hs_rng_categorical(hs_rng_t *rng, const double *probs, int n);

/* --- Misc --- */
int hs_file_exists(const char *path);
double hs_clock_ms(void);

#endif /* SPECIESID_UTILS_H */
