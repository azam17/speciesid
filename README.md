# SpeciesID

A bias-aware expectation-maximization framework for quantitative species authentication in meat products via DNA metabarcoding.

## Overview

SpeciesID is a computational framework that provides calibrated weight fraction estimates of meat species from amplicon sequencing (metabarcoding) data. It jointly models mitochondrial copy number variation, PCR amplification bias, and DNA degradation within an EM algorithm.

**Key features:**
- Two-stage k-mer classification: FracMinHash (k=21) coarse screening + exact containment (k=21)
- Bias-aware EM with Dirichlet priors, DNA yield correction, PCR bias, and degradation modeling
- Likelihood ratio tests for species presence with confidence intervals
- Reference database with configurable species categories for screening and review
- <1 second processing time for 15,000 reads
- Command-line and native macOS GUI interfaces

## Building

```bash
make            # Build CLI
make gui        # Build macOS GUI (requires SDL2)
make test       # Build and run tests
```

Requires: C11 compiler, zlib. Optional: SDL2 (for GUI).

## Quick Start

```bash
# Build reference database and index
./speciesid build-db -o speciesid.db
./speciesid index -d speciesid.db -o speciesid.idx

# Classify a sample
./speciesid run -x speciesid.idx -r sample.fastq -f json -o result.json

# With post-EM pruning (recommended for short amplicons)
./speciesid run -x speciesid.idx -r sample.fastq --prune 0.05 -f json -o result.json
```

## Project Structure

```
src/              C source code (~7,000 lines)
  main.c          CLI entry point
  em.c/h          EM algorithm core
  classify.c/h    Two-stage k-mer classification
  kmer.c/h        K-mer operations
  index.c/h       Reference index construction
  refdb.c/h       Reference database management
  refseqs.h       Embedded reference sequences
  calibrate.c/h   Spike-in calibration
  simulate.c/h    Read simulation
  report.c/h      Output formatting
  utils.c/h       Utility functions
gui/              macOS GUI (SDL2 + Nuklear)
lib/              Third-party headers (khash, kseq, nuklear)
test/             Unit and integration tests
data/
  amplicons/      Reference marker sequences (COI, cytb, 16S)
  mitogenomes/    Complete mitochondrial genomes
  benchmark/      Ground truth files for real-data validation
scripts/          Benchmark and analysis scripts
  download_real_data.sh       Download validation datasets
  download_opsonx.sh          OPSON X dataset download + primer test
  enumerate_denay.sh          ENA API accession enumeration
  run_paper_benchmark.sh      Simulated mixture benchmarks
  run_real_data_benchmark.sh  Real-data benchmarks (multi-dataset)
  compute_metrics.py          Compute publication metrics
  extract_amplicons.py        Extract amplicon regions from genomes
benchmark_results/  Pre-computed benchmark TSV files
paper/              Manuscript (speciesid_paper.md)
```

## Validation

SpeciesID is validated on **174 real amplicon sequencing samples** from two independent studies:

| Dataset | Samples | BioProject | Reference |
|---------|---------|------------|-----------|
| Denay et al. (2023) | 79 | PRJEB57117 | [Foods 12(5):968](https://doi.org/10.3390/foods12050968) |
| OPSON X (Kappel et al., 2023) | 95 | PRJNA926813 | [J. Consum. Prot. Food Saf. 18:257](https://doi.org/10.1007/s00003-023-01437-w) |

### Reproducing benchmarks

```bash
# 1. Download real data (requires SRA Toolkit)
bash scripts/download_real_data.sh denay     # 79 samples from Denay et al.
bash scripts/download_real_data.sh opsonx    # 95 samples from OPSON X
bash scripts/download_real_data.sh all       # Both datasets

# 2. Run benchmarks
bash scripts/run_paper_benchmark.sh          # Simulated mixtures
bash scripts/run_real_data_benchmark.sh all  # Real data (both datasets)

# 3. Compute metrics
python3 scripts/compute_metrics.py
```

### Simulated benchmark results

| Metric | Value |
|--------|-------|
| MAE (binary mixtures) | 2.59 pp |
| R-squared | 0.977 |
| Detection F1 | 1.000 |
| Trace detection (0.5% at 500 reads/marker) | Sensitivity 1.00 |
| Processing time (15K reads) | 0.64 s |

## License

MIT License. See [LICENSE](LICENSE).

## Citation

If you use SpeciesID in your research, please cite:

> [Authors]. A bias-aware expectation-maximization framework for quantitative species authentication in meat products via DNA metabarcoding. [Journal, Year].
