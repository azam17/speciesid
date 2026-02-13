#include "degrade.h"
#include <math.h>

double degrade_estimate_lambda(const int *insert_sizes, int n) {
    if (n <= 0) return 0.0;
    /* MLE for exponential distribution: lambda = 1 / mean */
    double sum = 0.0;
    for (int i = 0; i < n; i++) sum += (double)insert_sizes[i];
    double mean = sum / (double)n;
    if (mean < 1.0) return 1.0; /* very degraded */
    return 1.0 / mean;
}

void degrade_survival_factors(double lambda, const int *amplicon_lengths,
                               int S, int M, double *survival) {
    for (int s = 0; s < S; s++) {
        for (int m = 0; m < M; m++) {
            int idx = s * M + m;
            survival[idx] = exp(-lambda * (double)amplicon_lengths[idx]);
        }
    }
}
