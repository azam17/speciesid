/* =============================================================================
 * SpeciesID Tunable Parameters
 * =============================================================================
 *
 * THIS IS THE AUTORESEARCH AGENT'S PLAYGROUND.
 *
 * Every constant in this file affects the F1 score on the validation benchmark.
 * The agent modifies ONLY this file. All other source files are immutable.
 *
 * Current baseline F1: 0.652 (Sensitivity 0.692, Precision 0.615)
 * Biggest F1 levers:
 *   - Sus_scrofa: 24 FN (pig at 10% in cattle mixtures, falls below threshold)
 *   - Dicentrarchus_labrax: 22 FP (16S/12S k-mer cross-reactivity, 2 markers)
 *
 * Constraints:
 *   - Must compile with: make clean && make
 *   - Must pass: make test
 *   - K-mer sizes are baked into the index — changing them requires rebuild
 *   - Species-unique subtraction at k=21 HURTS (too few unique k-mers for
 *     short 16S/12S refs). Only viable at k=13 for long reads.
 * ========================================================================== */

#ifndef SPECIESID_PARAMS_H
#define SPECIESID_PARAMS_H

/* --- Classification: Short-read (Illumina) defaults --- */
#define PARAM_MIN_CONTAINMENT_SHORT    0.30   /* Min containment to report hit */
#define PARAM_COARSE_THRESHOLD_SHORT   0.05   /* Coarse FracMinHash filter */

/* --- Classification: Long-read (generic PacBio/ONT) defaults --- */
#define PARAM_MIN_CONTAINMENT_LONG     0.02   /* Much lower for error-prone reads */
#define PARAM_COARSE_THRESHOLD_LONG    0.01

/* --- Classification: Nanopore-specific defaults --- */
#define PARAM_MIN_CONTAINMENT_NANO     0.10
#define PARAM_COARSE_THRESHOLD_NANO    0.02

/* --- Long-read sliding window --- */
#define PARAM_LR_WINDOW_SIZE           300    /* Window size in bp */
#define PARAM_LR_WINDOW_STRIDE         150    /* Stride in bp */
#define PARAM_LR_MODE_THRESHOLD        200    /* Median read length > this → long-read mode */

/* --- Index k-mer sizes --- */
#define PARAM_COARSE_K                 21     /* FracMinHash coarse screening */
#define PARAM_FINE_K                   21     /* Exact k-mer fine matching */
#define PARAM_FMH_SCALE                0.01   /* 1% of hash space */

/* --- Long-read index k-mer sizes (smaller for error tolerance) --- */
#define PARAM_LR_COARSE_K              11
#define PARAM_LR_FINE_K                13
#define PARAM_LR_PRIMER_K              9

/* --- EM algorithm --- */
#define PARAM_EM_MAX_ITER              200
#define PARAM_EM_CONV_THRESHOLD        1e-6
#define PARAM_EM_N_RESTARTS            3
#define PARAM_EM_ALPHA                 0.05   /* Dirichlet sparsity prior on w (lower = sparser) */
#define PARAM_EM_D_MU                  0.0    /* LogNormal prior mean on DNA yield */
#define PARAM_EM_D_SIGMA               0.5    /* LogNormal prior stddev on DNA yield */
#define PARAM_EM_B_MU                  0.0    /* LogNormal prior mean on PCR bias */
#define PARAM_EM_B_SIGMA               0.5    /* LogNormal prior stddev on PCR bias */
#define PARAM_EM_MIN_CONTAINMENT       0.1    /* EM-level containment floor */
#define PARAM_EM_SEED                  42
#define PARAM_EM_PRUNE_THRESHOLD       0.0    /* Post-EM pruning weight floor (0=disabled) */

/* --- Reporting filters --- */
#define PARAM_WEIGHT_FLOOR             3.0    /* Species with weight < this AND read_pct < this: skip */
#define PARAM_READ_PCT_FLOOR           3.0
#define PARAM_MIN_MARKERS_FOR_LOW_WT   3      /* Species on fewer markers AND weight < ceiling: suppress */
#define PARAM_LOW_WT_CEILING           5.0    /* Weight ceiling for marker-count filter */

#endif /* SPECIESID_PARAMS_H */
