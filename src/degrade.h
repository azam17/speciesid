#ifndef SPECIESID_DEGRADE_H
#define SPECIESID_DEGRADE_H

/* Estimate degradation rate from insert size distribution.
 * Models fragment length as exponential decay: P(L) ~ exp(-lambda * L)
 * Returns lambda_proc (degradation rate) via MLE.
 */
double degrade_estimate_lambda(const int *insert_sizes, int n);

/* Compute survival factors: P(amplicon survives processing)
 * survival[s*M+m] = exp(-lambda * amplicon_length[s*M+m])
 * S = number of species, M = number of markers
 */
void degrade_survival_factors(double lambda, const int *amplicon_lengths,
                               int S, int M, double *survival);

#endif /* SPECIESID_DEGRADE_H */
