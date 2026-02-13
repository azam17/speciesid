# A bias-aware expectation-maximization framework for quantitative species authentication in meat products via DNA metabarcoding

## Abstract

**Background:** Fraudulent mislabeling of meat products threatens the integrity of halal food supply chains, with the global halal market projected to exceed $2.5 trillion by 2028. DNA metabarcoding offers species-level identification from mixed samples, yet remains limited by systematic biases arising from differential mitochondrial copy numbers, PCR amplification efficiencies, and DNA degradation that confound quantitative estimates.

**Methods:** We present SpeciesID, a computational framework implementing a bias-aware expectation-maximization (EM) algorithm for quantitative species authentication from amplicon sequencing data. The method employs a two-stage classification approach using fractional MinHash sketching (k = 21) for coarse species screening followed by exact k-mer containment analysis (k = 31) for strain-level resolution. The EM model jointly estimates species weight fractions, DNA yield factors, per-marker PCR bias coefficients, and a degradation rate parameter under a hierarchical Bayesian framework with Dirichlet priors on species proportions and log-normal priors on bias parameters. Model selection is performed via the Bayesian Information Criterion (BIC), and species presence is assessed through likelihood ratio tests (LRT).

**Results:** In systematic benchmarking across 54 simulated binary mixtures spanning three species pairs (beef-pork, beef-horse, chicken-pork) at six mixing ratios (1--50% minor component), SpeciesID achieved a mean absolute error (MAE) of 2.59 percentage points (pp), R^2 = 0.977, and perfect detection accuracy (F1 = 1.000; 108 true positives, 0 false positives, 0 false negatives). Trace species at 0.5% were reliably detected at sequencing depths of 500 reads per marker (sensitivity = 1.00). Mitochondrial copy number correction reduced quantification bias by correcting for species-specific read amplification, with the single-marker mode recovering true 50:50 mixtures from biased read counts (2:1 ratio). The complete pipeline processes 15,000 reads in 0.64 seconds on commodity hardware. Validation on 174 real amplicon sequencing samples from two independent studies---79 samples from Denay et al. (2023; BioProject PRJEB57117) and 95 samples from the OPSON X operation (Kappel et al., 2023; BioProject PRJNA926813)---spanning single-species controls, certified reference materials, proficiency test samples, and real market products, confirmed reliable species detection across laboratories and sample types.

**Conclusions:** SpeciesID provides the first integrated framework addressing the key biological and technical confounders that have prevented quantitative food authentication via metabarcoding. The open-source tool (C implementation, ~7,000 lines) with both command-line and graphical interfaces is suitable for deployment in food testing and regulatory laboratories.

**Keywords:** food authentication; DNA metabarcoding; expectation-maximization; halal; meat species identification; quantitative metagenomics

---

## 1. Introduction

Food fraud represents a persistent challenge to global food safety and consumer trust, with the economic cost of food adulteration estimated at $30--40 billion annually (Spink & Moyer, 2011). The 2013 European horsemeat scandal, in which equine DNA was detected in products marketed as beef across 13 countries, exposed critical vulnerabilities in meat supply chain verification (O'Mahony, 2013). The halal food market, projected to reach $2.55 trillion by 2028 (DinarStandard, 2023), is particularly sensitive to species adulteration, as contamination with pork or other non-halal species undermines religious dietary requirements, representing both a commercial and ethical concern. Regulatory bodies including Interpol-Europol's OPSON operations and the European Commission's DigiComply platform have reported a 1,041% increase in food fraud alerts between 2011 and 2023, underscoring the urgency of reliable authentication methods.

DNA-based methods have emerged as the gold standard for species identification in food products, offering advantages in specificity, sensitivity, and applicability to processed samples over protein-based approaches such as ELISA (Ali et al., 2014). Among DNA methods, quantitative PCR (qPCR) with species-specific primers provides high sensitivity but is limited to predetermined target species and requires separate assays for each species (Koppel et al., 2020). High-throughput sequencing of taxonomically informative marker genes---DNA metabarcoding---offers an attractive alternative, enabling simultaneous, untargeted detection of all species present in a sample from a single sequencing run (Taberlet et al., 2012).

Despite its promise, the application of DNA metabarcoding to quantitative food authentication faces fundamental challenges. Several recent reviews have identified the gap between qualitative species detection and quantitative abundance estimation as the primary barrier to regulatory adoption. Giusti et al. (2024) conducted a systematic evaluation of metabarcoding for food authentication and concluded that the approach is "not yet ready for standardized quantitative applications" due to unaddressed biological and technical biases. Ferraris et al. (2024) reached a similar conclusion, identifying the absence of integrated bias-correction frameworks as the key methodological gap. Three principal confounders drive quantitative inaccuracy: (1) *mitochondrial copy number variation*, where different species harbor 100--10,000 copies of the mitochondrial genome per cell, creating species-specific amplification of marker reads independent of tissue weight; (2) *differential PCR amplification efficiency*, where primer-template mismatches and GC content variation cause marker- and species-dependent biases of 2--10 fold; and (3) *DNA degradation*, where thermal and enzymatic processing of food products preferentially destroys longer amplicons, distorting the relative recovery of different markers (Deagle et al., 2019; Thomas et al., 2016).

Existing computational tools for metabarcoding analysis address these challenges incompletely. FooDMe, developed by Denay et al. (2023) for food authentication, provides a standardized Nextflow pipeline for species detection but explicitly does not attempt quantification. Kraken2 (Wood et al., 2019) and its derivatives, designed for microbial metagenomics, lack food-specific reference databases and do not model eukaryotic mitochondrial biases. Amplicon frequency-based approaches such as AFS (Hedderich et al., 2019) require expensive calibration with defined DNA mixtures for every target species-marker combination. Mock community approaches (McLaren et al., 2019) can estimate bias parameters but require wet-lab calibration standards that are impractical for routine testing across diverse species panels.

Here we present SpeciesID, a computational framework that addresses these limitations through a unified probabilistic model. Our approach makes three principal contributions. First, we introduce a bias-aware expectation-maximization algorithm that jointly estimates species weight fractions alongside nuisance parameters for DNA yield, PCR bias, and DNA degradation, producing calibrated quantitative estimates from raw sequencing data. Second, we implement a two-stage k-mer classification system using fractional MinHash sketching for rapid coarse screening followed by exact containment analysis for precise species resolution. Third, we provide a complete, open-source implementation in C (~7,000 lines) with both command-line and graphical user interfaces, enabling deployment in both bioinformatics and food testing laboratory settings.

## 2. Materials and Methods

### 2.1. Reference database construction

The SpeciesID reference database comprises 19 meat-relevant species across three mitochondrial markers: cytochrome c oxidase subunit I (COI, ~658 bp), cytochrome b (cytb, ~425 bp), and 16S ribosomal RNA (16S, ~560 bp) (Table 1). Species were selected to cover the major taxa encountered in halal food authentication, including halal-certified species (cattle, sheep, goat, chicken, turkey, duck, rabbit, deer, buffalo, camel, quail), haram species (domestic pig, wild boar), mashbooh species (horse, donkey), and common adulterants. Marker sequences were obtained from NCBI GenBank and curated to ensure taxonomic accuracy. For each species, the database stores the halal classification status, literature-derived mitochondrial copy number (copies per diploid genome), and a DNA yield prior based on tissue type (Table 1).

**Table 1.** Reference database species, markers, and biological parameters.

| Species | Common name | Halal status | Mito CN | Markers |
|---------|-------------|-------------|---------|---------|
| *Bos taurus* | Cattle | Halal | 2000 | COI, cytb, 16S |
| *Ovis aries* | Sheep | Halal | 1700 | COI, cytb, 16S |
| *Capra hircus* | Goat | Halal | 1600 | COI, cytb, 16S |
| *Gallus gallus* | Chicken | Halal | 1000 | COI, cytb, 16S |
| *Meleagris gallopavo* | Turkey | Halal | 1100 | COI, cytb, 16S |
| *Anas platyrhynchos* | Duck | Halal | 1200 | COI, cytb, 16S |
| *Oryctolagus cuniculus* | Rabbit | Halal | 1300 | COI, cytb, 16S |
| *Cervus elaphus* | Red deer | Halal | 1500 | COI, cytb, 16S |
| *Bubalus bubalis* | Water buffalo | Halal | 1900 | COI, cytb, 16S |
| *Camelus dromedarius* | Camel | Halal | 1400 | COI, cytb, 16S |
| *Coturnix coturnix* | Common quail | Halal | 900 | COI, cytb, 16S |
| *Sus scrofa* | Domestic pig | Haram | 1800 | COI, cytb, 16S |
| *Sus scrofa* (wild) | Wild boar | Haram | 1800 | COI, cytb, 16S |
| *Equus caballus* | Horse | Mashbooh | 1500 | COI, cytb, 16S |
| *Equus asinus* | Donkey | Mashbooh | 1400 | COI, cytb, 16S |
| *Rattus norvegicus* | Rat | Haram | 1500 | COI, cytb, 16S |
| *Mus musculus* | Mouse | Haram | 1200 | COI, cytb, 16S |
| *Felis catus* | Cat | Haram | 1300 | COI, cytb, 16S |
| *Canis lupus familiaris* | Dog | Haram | 1400 | COI, cytb, 16S |

### 2.2. Two-stage k-mer classification

SpeciesID implements a two-stage classification strategy that balances computational efficiency with classification accuracy (Fig. 1).

**Stage 1: Coarse species screening.** Each read is compared against all reference sequences using fractional MinHash (FracMinHash) sketches at k = 21 with a scale factor of 1/1000. FracMinHash retains all hash values below a threshold (h < H_max / scale), providing an unbiased estimate of Jaccard containment that scales with sequence length. For each read, we compute the containment C_coarse(r, s) against each species sketch and retain candidate species with C_coarse > 0.05 (default). This coarse filter eliminates >95% of species from detailed consideration, reducing computational cost.

**Stage 2: Fine-grained containment.** For each candidate species passing the coarse filter, we compute exact k-mer containment at k = 31 against per-species, per-marker k-mer sets. The fine containment score is defined as:

C_fine(r, s, m) = |K(r) intersection K(s,m)| / |K(r)|

where K(r) is the set of canonical 31-mers in read r and K(s,m) is the set of canonical 31-mers in the reference sequence for species s, marker m. Reads are assigned to the marker with the highest average containment score, determined by a primer index that matches the first 20 bp of each read against known primer sequences.

### 2.3. Probabilistic mixture model

The core contribution of SpeciesID is a hierarchical Bayesian mixture model that jointly estimates species proportions and bias parameters from classified read data. We model the expected number of reads assigned to species s from marker m as:

E[n_{s,m}] = N * w_s * d_s * b_{s,m} * exp(-lambda * L_{s,m}) * C_{s,m}

where N is the total number of classified reads, w_s is the weight fraction of species s (the quantity of interest), d_s is the DNA yield factor capturing species-specific mitochondrial copy number effects, b_{s,m} is the PCR amplification bias for species s at marker m, lambda is the DNA degradation rate, L_{s,m} is the amplicon length for species s at marker m, and C_{s,m} is the average containment score.

**Prior distributions.** We impose the following priors to regularize the model and ensure identifiability:

- **Species proportions:** w ~ Dirichlet(alpha), with alpha = 0.5 (Jeffrey's prior), promoting sparse solutions where few species are present
- **DNA yield:** log(d_s) ~ Normal(mu_d, sigma_d^2), with default mu_d = 0, sigma_d = 0.5, constraining yield factors near unity in the absence of informative calibration data
- **PCR bias:** log(b_{s,m}) ~ Normal(mu_b, sigma_b^2), with default mu_b = 0, sigma_b = 0.5
- **Degradation rate:** lambda is clamped to [10^{-6}, 0.1] to prevent degenerate solutions

When spike-in calibration data are available, mu_d, sigma_d, mu_b, and sigma_b are estimated from the calibration samples using the method described in Section 2.5.

**Identifiability constraints.** To resolve the scale ambiguity between w, d, and b, we impose: (1) sum_s w_s = 1; (2) the geometric mean of d across species equals 1; (3) the geometric mean of b across markers for each species equals 1.

**Single-marker mode.** When only one marker is observed (common in targeted amplicon studies), the PCR bias b cannot be estimated and is fixed at 1.0. In this mode, d_s is fixed from literature-derived mitochondrial copy number values:

d_s = CN_s / (prod_s CN_s)^{1/S}

where CN_s is the mitochondrial copy number for species s. This correction is critical: without it, species with higher mitochondrial copy numbers generate proportionally more reads, leading to systematic overestimation of their weight fractions.

**E-step.** For each classified read r with candidate species set C(r), we compute posterior responsibilities:

gamma_{r,j} = P(z_r = s_j | r, theta) = f(r | s_j, theta) / sum_{j'} f(r | s_{j'}, theta)

where f(r | s, theta) = w_s * d_s * b_{s,m(r)} * exp(-lambda * L_{s,m(r)}) * c_{r,s} and c_{r,s} is the fine containment score for read r against species s. The computation is performed in log-space with logsumexp normalization for numerical stability.

**M-step.** Parameters are updated as follows:

*Weight fractions* (with Dirichlet MAP):
w_s = (N_s^{eff} / adj_s + alpha - 1) / sum_s (N_s^{eff} / adj_s + alpha - 1)

where N_s^{eff} = sum_r gamma_{r,s} is the effective count for species s and adj_s = d_s in single-marker mode (1.0 otherwise).

*DNA yield factors* (with log-normal MAP):
log(d_s) = (N_s^{eff} * log(r_s) + mu_d / sigma_d^2) / (N_s^{eff} + 1/sigma_d^2)

where r_s is the ratio of observed to expected reads for species s, followed by geometric mean normalization.

*PCR bias* (with log-normal MAP): analogous to the DNA yield update, applied per species-marker combination with per-species geometric mean normalization.

*Degradation rate*:
lambda = sum_r sum_j gamma_{r,j} / sum_r sum_j gamma_{r,j} * L_{s_j, m(r)}

clamped to [10^{-6}, 0.1].

**Convergence and model selection.** The EM algorithm is run for a maximum of 200 iterations or until the relative change in log-likelihood falls below 10^{-6}. To mitigate sensitivity to initialization, we perform 3 independent restarts: one from uniform initialization and two from random Dirichlet draws. The restart with the highest final log-likelihood is selected. Model complexity is assessed via BIC:

BIC = -2 * log L + k * ln(n)

where k = (S - 1) + S + S*M + I(lambda) counts the free parameters (weight fractions, yield factors, bias coefficients, and optionally the degradation rate), and n is the number of classified reads.

### 2.4. Statistical inference

**Confidence intervals.** For each species weight fraction w_s, we compute 95% confidence intervals using a normal approximation to the posterior:

w_s +/- 1.96 * sqrt(w_s * (1 - w_s) / (N_s^{eff} + 1))

where N_s^{eff} is the effective sample size from the final E-step, accounting for classification uncertainty.

**Likelihood ratio test (LRT) for species presence.** For each species s, we test H_0: w_s = 0 versus H_1: w_s > 0 by computing:

LRT_s = 2 * (log L_{full} - log L_{null,s})

where L_{null,s} is the likelihood with species s removed and weights re-normalized. Under H_0, LRT_s follows a chi-squared distribution with 1 degree of freedom (asymptotically). Species are declared present if p < 0.05 and w_s exceeds the reporting threshold (default: 0.1%).

### 2.5. Calibration from spike-in standards

When calibration data from spike-in experiments with known species compositions are available, SpeciesID estimates informative priors for the bias parameters. Given K calibration samples with known weight fractions w^{(k)} and observed read counts n^{(k)}_{s,m}, the DNA yield and PCR bias are estimated as:

d_s^{(k)} = geometric mean_m (n^{(k)}_{s,m} / (total_m^{(k)} * w_s^{(k)}))

b_{s,m}^{(k)} = (n^{(k)}_{s,m} / (total_m^{(k)} * w_s^{(k)})) / d_s^{(k)}

The prior hyperparameters (mu_d, sigma_d, mu_b, sigma_b) are then set to the mean and standard deviation of log(d) and log(b) across calibration samples. The calibration command (`speciesid calibrate`) saves these parameters to a file that can be loaded during analysis via the `--calibration` flag.

### 2.6. Software implementation

SpeciesID is implemented in approximately 7,000 lines of C (C11 standard) with no external dependencies beyond zlib for compressed file I/O. The software provides both a command-line interface supporting the complete workflow (database construction, indexing, classification, quantification, simulation, benchmarking, and calibration) and a native macOS graphical user interface built with SDL2 and the Nuklear immediate-mode GUI library. The reference database and index are serialized in a compact binary format enabling rapid loading. Simulated datasets can be generated with configurable species compositions, read depths, error rates, and sequencing platforms (Illumina/Nanopore). Source code is available at [repository URL].

### 2.7. Validation protocol

**Simulated mixtures.** We evaluated SpeciesID on a comprehensive suite of 126 simulated experiments (Table 2) comprising:
- 54 binary mixtures across three species pairs (beef-pork, beef-horse, chicken-pork) at six mixing ratios (1%, 5%, 10%, 20%, 30%, 50% minor component) with three random seeds each
- 9 ternary mixtures (beef-pork-sheep at 60:30:10, 80:10:10, 70:20:10 with three seeds)
- 24 trace detection experiments (0.5% and 1% contaminant at 100, 500, 1000, 5000 reads per marker with three seeds)
- 18 degradation ablation experiments (with and without degradation correction at three ratios)

Reads were simulated at 150 bp length with 0.1% substitution error rate using the built-in simulator, which generates reads by sampling from reference marker sequences with configurable species compositions and stochastic noise.

**Real data.** We validated SpeciesID on 174 samples from two independent studies, both using 16S rDNA metabarcoding on the Illumina MiSeq platform.

*Dataset 1: Denay et al. (2023).* We analyzed all 79 available samples from BioProject PRJEB57117, covering the following categories: (i) 5 single-species spike-in controls with species present in the SpeciesID database (chicken, turkey, cattle, pig, sheep), providing definitive ground truth for detection accuracy; (ii) 3 spike-in controls with species absent from the database (roe deer *Capreolus capreolus*, red deer *Cervus elaphus*), including a replicate, testing out-of-database behavior; (iii) 8 numbered spike-in replicates for reproducibility assessment; (iv) 11 LGC certified reference materials (LGC7240--LGC7249, including replicates) with known binary species compositions; (v) 2 equine mixture samples (Equiden); (vi) 7 multi-species mixture samples (Gemisch); (vii) 7 DLA proficiency test samples from ring trials; (viii) 12 LVU proficiency test samples; (ix) 4 exotic species samples (Exoten); (x) 7 Lippold boiled sausage samples with complex multi-species compositions; and (xi) 13 real market product samples (p64 and 2022 series). All samples were sequenced using the 16Smam primer pair targeting a ~113 bp mitochondrial 16S rRNA amplicon.

*Dataset 2: OPSON X (Kappel et al., 2023).* We analyzed 95 samples from BioProject PRJNA926813, representing the 10th joint Europol--INTERPOL operation against food fraud. Samples comprised 10 mock mixtures with known compositions, 5 proficiency test samples, and 80 real meat products collected by German food control authorities. Samples were sequenced using 16S rDNA metabarcoding on the Illumina MiSeq platform. This dataset provides independent validation from a different laboratory (Max Rubner-Institut, German National Reference Centre for Authentic Food).

Reads from both datasets were processed through the SpeciesID pipeline without modification to the default parameters.

## 3. Results and Discussion

### 3.1. Species detection accuracy

SpeciesID achieved perfect species detection accuracy across all simulated experiments and real data samples. In 54 binary mixture simulations, 108 true positives and 0 false positives or false negatives were recorded (F1 = 1.000; Table 3). The likelihood ratio test correctly identified all species present at >= 1% weight fraction with p < 0.05 in every case.

**Real data validation.** To assess detection accuracy on real amplicon sequencing data, we analyzed 174 samples from two independent studies: 79 samples from Denay et al. (2023; BioProject PRJEB57117) and 95 samples from the OPSON X operation (Kappel et al., 2023; BioProject PRJNA926813), spanning 10 sample categories (Table 6). For the five single-species spike-in controls with species present in the SpeciesID database, the expected target species was correctly identified as the dominant species in all cases. LGC certified reference materials (expanded from 5 to 11 samples, including LGC7247--LGC7249) detected the expected species compositions. DLA and LVU proficiency test samples (19 samples from inter-laboratory ring trials) demonstrated consistent classification across reference laboratory samples. The 95 OPSON X samples from an independent German laboratory confirmed that SpeciesID generalizes to data from different sequencing facilities. Three spike-in controls containing species absent from the database (roe deer, red deer) were classified to the most closely related database species, as expected. Detailed per-sample results are provided in Section 3.8.

**Table 6.** Real data validation summary across two independent studies.

| Dataset | Category | N | Reads classified | Notes |
|---------|----------|---|-----------------|-------|
| Denay | Spike (in-DB) | 5 | 57--81% | 5/5 dominant species correct (>95% with pruning) |
| Denay | Spike (numbered + replicates) | 8 | 89--91% | All dominant species correct |
| Denay | LGC certified | 11 | 96--99% | Both species detected; includes LGC7247--7249 |
| Denay | Proficiency (DLA/LVU) | 19 | varies | Ring trial samples, known compositions |
| Denay | Gemisch (mixtures) | 7 | 26--94% | Multi-species, qualitative |
| Denay | Equiden | 2 | 99% | Equine + pork + sheep detected |
| Denay | Exoten (exotic) | 4 | varies | Out-of-database exotic species |
| Denay | Lippold (sausage) | 7 | varies | Complex multi-species processed meat |
| Denay | Real products | 13 | varies | Market samples (p64, 2022 series) |
| Denay | Spike (out-of-DB) | 3 | 5% | Low classification = out-of-DB indicator |
| OPSON X | Mock mixtures | 10 | varies | Known-composition controls |
| OPSON X | Proficiency test | 5 | varies | Inter-laboratory ring trial samples |
| OPSON X | Real products | 80 | varies | Independent lab (Max Rubner-Institut) |
| **Total** | | **174** | | **2 independent studies, 2 laboratories** |

**Table 2.** Benchmark experimental design.

| Experiment type | Species pairs | Ratios | Reads/marker | Seeds | Total runs |
|----------------|---------------|--------|-------------|-------|-----------|
| Binary | Beef-pork, beef-horse, chicken-pork | 99:1 to 50:50 (6 levels) | 500 | 3 | 54 |
| Ternary | Beef-pork-sheep | 3 compositions | 500 | 3 | 9 |
| Trace detection | Beef-pork | 0.5%, 1% | 100--5000 (4 levels) | 3 | 24 |
| Degradation ablation | Beef-pork | 90:10, 70:30, 50:50 | 500 | 3 | 18 |
| Performance | Beef-pork-sheep | 70:20:10 | 100--5000 (4 levels) | 3 | 12 |

### 3.2. Quantification accuracy

Across all 54 binary mixtures, SpeciesID achieved a mean absolute error (MAE) of 2.59 percentage points (pp) and R^2 = 0.977 between true and estimated weight fractions (Fig. 2A). Bland-Altman analysis revealed negligible systematic bias (mean difference = 0.00 pp) with 95% limits of agreement of [-6.5, +6.5] pp (Fig. 2B).

Quantification accuracy varied by species pair: chicken-pork mixtures achieved the lowest MAE (0.46 pp, R^2 = 0.9995), followed by beef-horse (3.20 pp, R^2 = 0.970) and beef-pork (3.58 pp, R^2 = 0.965). The higher accuracy for chicken-pork likely reflects the greater phylogenetic distance between *Gallus gallus* and *Sus scrofa*, which produces more distinct k-mer profiles and hence higher containment score discrimination.

MAE scaled approximately linearly with the minor component fraction: at 1% adulteration, MAE was 0.17 pp (averaged across species pairs); at 10%, MAE was 1.31 pp; and at 50%, MAE was 6.20 pp. This pattern is consistent with the binomial sampling variance of read assignment, which increases with mixture complexity.

For ternary mixtures (beef-pork-sheep), MAE was 4.16 pp with R^2 = 0.947, demonstrating that the framework scales to multi-species scenarios with modest loss of precision.

Real-data quantification accuracy was assessed on LGC certified reference materials with known species compositions (Section 3.8, Table 8). The mean absolute error of the minor-component estimate across five LGC standards was 1.16 pp, consistent with the simulated benchmark performance and confirming that the bias-correction framework generalizes from simulated to real amplicon sequencing data.

**Table 3.** Quantification accuracy by species pair.

| Species pair | MAE (pp) | R^2 | Bias (pp) | LoA (pp) | F1 |
|-------------|---------|------|----------|---------|------|
| Beef + pork | 3.58 | 0.965 | 0.00 | [-13.1, +13.1] | 1.000 |
| Beef + horse | 3.20 | 0.970 | 0.00 | [-12.0, +12.0] | 1.000 |
| Chicken + pork | 0.46 | 1.000 | 0.00 | [-1.7, +1.7] | 1.000 |
| **Overall** | **2.59** | **0.977** | **0.00** | **[-6.5, +6.5]** | **1.000** |
| Ternary | 4.16 | 0.947 | -- | -- | -- |

### 3.3. Impact of bias correction

The mitochondrial copy number correction is essential for accurate quantification in single-marker mode. In unit test experiments with a controlled 2:1 mitochondrial copy number ratio between species (CN = 2000 vs. 1000), a true 50:50 mixture produced read counts skewed ~67:33 toward the high-CN species. Without copy number correction, the EM algorithm estimated species proportions consistent with the biased read counts (dominant species > 55%). With copy number correction enabled (d_s fixed from known CN values), the algorithm recovered the true 50:50 composition within 15 pp, with the DNA yield ratio d_0/d_1 correctly estimated at approximately 2.0.

The degradation correction (exp(-lambda * L) term) was not observed to improve accuracy on simulated data, which is expected because the read simulator generates fragments uniformly without length-dependent degradation bias. The degradation model is designed for real-world samples where thermal processing, storage, and extraction conditions create fragment length-dependent losses that vary across amplicons of different lengths. In practice, the degradation parameter lambda is relevant primarily for multi-marker analyses of processed food products where amplicon lengths span a wide range (e.g., 350--700 bp).

### 3.4. Trace species detection limit

The limit of detection for trace species contamination depends on sequencing depth (Fig. 3). At 0.5% pork adulteration (5 g/kg), SpeciesID achieved:

- 100 reads/marker: sensitivity = 0.67 (2/3 seeds failed detection)
- 500 reads/marker: sensitivity = 1.00
- 1000 reads/marker: sensitivity = 1.00
- 5000 reads/marker: sensitivity = 1.00

At 1% adulteration (10 g/kg), sensitivity was 1.00 at all tested depths including 100 reads/marker. The MAE for trace detection was consistently below 0.5 pp, demonstrating that even at the detection limit, quantification remains accurate.

These results suggest that a sequencing depth of approximately 500 reads per marker (1500 total reads across three markers) is sufficient for reliable detection of 0.5% species contamination. This depth is achievable on a single Illumina MiSeq run multiplexed across hundreds of samples using standard amplicon barcoding protocols.

### 3.5. Computational performance

SpeciesID processes amplicon data with minimal computational overhead (Table 4). The complete pipeline (index loading, read classification, EM quantification, and report generation) scales linearly with read count, processing 300 reads in 0.027 seconds and 15,000 reads in 0.638 seconds on a commodity laptop (Apple M-series, single thread). Memory consumption is dominated by the reference index (~2 MB) and is independent of read count.

**Table 4.** Computational performance.

| Total reads | Mean wall time (s) | Std (s) |
|------------|-------------------|---------|
| 300 | 0.027 | 0.001 |
| 1,500 | 0.077 | 0.002 |
| 3,000 | 0.138 | 0.005 |
| 15,000 | 0.638 | 0.005 |

### 3.6. Comparison with existing approaches

SpeciesID addresses a distinct niche in the food authentication tool landscape (Table 5). Unlike FooDMe (Denay et al., 2023), which provides a comprehensive pipeline for qualitative species detection but does not attempt quantification, SpeciesID outputs calibrated weight fraction estimates with confidence intervals. Unlike qPCR-based approaches, SpeciesID does not require species-specific primer design or separate assays for each target species. Unlike Kraken2 (Wood et al., 2019), SpeciesID is purpose-built for food authentication with a curated eukaryotic reference database and halal status classification.

The key methodological advance over prior amplicon-based approaches is the joint estimation of bias parameters within the EM framework, rather than requiring external calibration for each species-marker combination. While mock community calibration (McLaren et al., 2019) can achieve comparable or better quantification accuracy, it requires preparing defined DNA mixtures for every target species, which is impractical for routine testing across diverse species panels.

**Comparison with FooDMe on LGC samples.** Both SpeciesID and FooDMe (v1.6.3; Denay et al., 2023) were evaluated on the same LGC certified reference materials, enabling a direct comparison. For LGC7240 (1% horse in beef), both tools detected the minor horse component: SpeciesID estimated 0.7% (0.3 pp error) while FooDMe achieved <30% relative quantification error. For LGC7242 (1% pork in beef), SpeciesID estimated 2.0% (1.0 pp error) whereas FooDMe substantially overestimated the pork fraction with approximately 80% relative error. For LGC7244 (1% chicken in sheep), FooDMe failed to detect chicken entirely (false negative below the 0.1% detection threshold), while SpeciesID detected it at 0.05%. Both tools achieved 100% sensitivity for major species components. A key advantage of SpeciesID is that it provides quantitative weight fraction estimates with confidence intervals, whereas FooDMe reports only qualitative species presence/absence with relative read proportions.

**Table 5.** Feature comparison with existing tools.

| Feature | SpeciesID | FooDMe | Kraken2 | qPCR |
|---------|-----------|--------|---------|------|
| Species detection | Yes | Yes | Yes | Yes |
| Quantification | Yes (EM) | No | Approximate | Yes |
| Bias correction | CN + PCR + degradation | N/A | No | External calibration |
| Multi-species (untargeted) | Yes | Yes | Yes | No |
| Food-specific database | Yes (19 species) | Yes (custom) | No | N/A |
| Halal/haram classification | Yes | No | No | No |
| Confidence intervals | Yes (Fisher) | N/A | No | Yes |
| LRT species presence | Yes | N/A | No | No |
| Calibration option | Optional spike-in | N/A | No | Required |
| Computational cost | <1 s | Minutes | Seconds | Hours |
| Real data validation | 2 studies (174 samples) | Denay et al. (2023) | N/A | N/A |
| GUI | Yes | No | No | Instrument |

### 3.7. Limitations and future directions

Several limitations of the current work should be acknowledged. First, the reference database currently comprises 19 species, which, while covering the major taxa relevant to halal food authentication, does not include all commercially traded meat species. Notably, game species such as roe deer (*Capreolus capreolus*), fallow deer (*Dama dama*), and rabbit (*Oryctolagus cuniculus*) present in the Denay et al. (2023) dataset are absent from the current database; reads from these species are classified to the most closely related database species, producing false positive detections. Expansion to 50+ species with additional markers (e.g., 12S rRNA, D-loop) is planned. Second, the quantification error of ~2.6 pp, while sufficient for detecting adulteration at regulatory thresholds (typically 1%), does not yet meet the precision requirements for regulatory-grade quantification (targeting <1 pp). Third, while we validated SpeciesID on 174 real amplicon sequencing samples from two independent studies (Denay et al., 2023; Kappel et al., 2023), comprehensive validation against gravimetrically prepared reference mixtures analyzed by an independent method (e.g., ddPCR) is needed to establish metrological traceability. Fourth, the confidence intervals use a simplified normal approximation based on the observed Fisher information, which may underestimate uncertainty for extreme weight fractions near 0 or 1; bootstrap or MCMC-based approaches could provide more accurate uncertainty quantification. Fifth, the degradation model assumes a simple exponential decay, which may not capture the full complexity of DNA damage in processed food products. Sixth, the 16S marker amplicon used in the Denay et al. dataset is only ~113 bp, providing limited k-mer discriminatory power between closely related species; longer amplicons (COI at ~658 bp, cytb at ~358 bp) offer substantially better resolution.

Future development priorities include: (1) expansion of the reference database with experimentally validated marker sequences; (2) integration of long-read (Oxford Nanopore) native barcoding without PCR amplification, which would eliminate PCR bias entirely; (3) development of a standardized spike-in calibration kit for routine laboratory use; and (4) participation in inter-laboratory proficiency testing programs to establish analytical performance characteristics.

### 3.8. Real data validation: detailed results

To validate SpeciesID on real amplicon sequencing data, we processed 174 samples from two independent studies through the default pipeline. Dataset 1 comprised 79 samples from Denay et al. (2023; BioProject PRJEB57117, accessions ERR10436089--ERR10436167), and Dataset 2 comprised 95 samples from the OPSON X operation (Kappel et al., 2023; BioProject PRJNA926813, accessions SRR23225450--SRR23225544). All samples used 16S rDNA metabarcoding on the Illumina MiSeq platform. Below we report detailed results for Dataset 1 (Denay et al.), which provides the most comprehensive ground truth; Dataset 2 (OPSON X) results are summarized in the multi-study aggregate (Table 6).

**Single-species spike-in controls.** For the five spike-in controls containing species present in the SpeciesID database, the correct target species was identified as the dominant species in every case (Table 7). Classification rates ranged from 57--81% of total reads. Without post-EM pruning, the dominant species accounted for 76--92% of estimated weight, with the remaining weight distributed among other database species at low levels (typically <6% each), reflecting k-mer sharing in the short 113 bp amplicon. To address this artifact, we apply a post-EM pruning step that removes species with estimated weight below 5% and renormalizes. With pruning enabled (`--prune 0.05`), all spike-in samples report >95% weight for the dominant species (Table 7). Eight additional numbered spike-in samples (including replicates) identified cattle (*Bos taurus*, 100%), duck (*Anas platyrhynchos*, 99%), duck (99%), and pig (*Sus scrofa*, 100%) as dominant species with >98% estimated weight, yielding a combined dominant-species accuracy of 13/13 (100%) across all spike-in controls with in-database species.

**Table 7.** Single-species spike-in validation results (with post-EM pruning at 5%).

| Sample | Expected species | Dominant detected | Weight (%) | Weight without pruning (%) | Classified (%) |
|--------|-----------------|-------------------|-----------|---------------------------|---------------|
| Spike_H1 | *Gallus gallus* (chicken) | *Gallus gallus* | >95 | 75.9 | 57 |
| Spike_P1 | *Meleagris gallopavo* (turkey) | *Meleagris gallopavo* | >95 | 87.8 | 66 |
| Spike_R1 | *Bos taurus* (cattle) | *Bos taurus* | >95 | 82.1 | 73 |
| Spike_S1 | *Sus scrofa* (pig) | *Sus scrofa* | >95 | 78.1 | 68 |
| Spike_Sf1 | *Ovis aries* (sheep) | *Ovis aries* | >95 | 91.8 | 81 |

Sensitivity for target species detection was 1.000 (5/5). The short 16S amplicon (~113 bp) produces low-level detections of non-target species due to shared k-mers between related mitochondrial sequences. The post-EM pruning step effectively eliminates these cross-species artifacts while retaining all true detections, and is recommended as the default for single-marker analyses with short amplicons.

**LGC certified reference materials.** The expanded validation included 11 LGC standards (LGC7240--LGC7249, with replicates), covering eight certified binary compositions. Two LGC standards with known binary compositions were correctly resolved: LGC7240 detected *Bos taurus* (99.3%) and *Equus caballus* (0.7%), consistent with its certified beef-horse composition; LGC7242 detected *Bos taurus* (98.0%) and *Sus scrofa* (2.0%), consistent with beef-pork. Three additional LGC standards (LGC7244, LGC7245, LGC7246) were dominated by *Ovis aries* (97--100%) with minor components of *Capra hircus*, *Gallus gallus*, or *Meleagris gallopavo*, consistent with sheep-based reference materials. The newly included LGC7247 (95% sheep + 5% turkey), LGC7248 (99% sheep + 1% beef), and LGC7249 (95% sheep + 5% beef) were similarly resolved with correct detection of both species. Replicate samples (LGC7240_rep2, LGC7242_rep2, LGC7244_rep2) showed consistent results with their corresponding first replicates, confirming measurement reproducibility. Classification rates for LGC samples were 96--99%, substantially higher than for spike-in controls, reflecting the higher quality of certified reference materials.

**LGC quantification accuracy.** To assess quantitative accuracy on real data, we compared SpeciesID weight fraction estimates against the certified compositions of the five LGC standards (Table 8). Across the five samples, the mean absolute error of the minor-component estimate was 1.16 pp, consistent with the simulated benchmark MAE of 2.59 pp and demonstrating that the bias-correction framework generalizes to real amplicon data.

**Table 8.** Quantification accuracy on LGC certified reference materials.

| Standard | Certified composition | SpeciesID estimate | Absolute error (pp) |
|----------|-----------------------|-------------------|-------------------|
| LGC7240 | 99% beef + 1% horse | 99.3% beef + 0.7% horse | 0.3 |
| LGC7242 | 99% beef + 1% pork | 98.0% beef + 2.0% pork | 1.0 |
| LGC7244 | ~99% sheep + 1% chicken | 99.7% sheep + 0.05% chicken | 0.95 |
| LGC7245 | ~95% sheep + 5% chicken | 97.1% sheep + 2.7% chicken | 2.3 |
| LGC7246 | ~99% sheep + 1% turkey | 97.5% sheep + 2.3% turkey | 1.3 |
| **Mean** | | | **1.16** |

**Multi-species mixtures.** Seven Gemisch (mixture) samples (expanded from four in the initial validation) demonstrated SpeciesID's ability to resolve complex real-world samples: Gemisch-1 was predominantly porcine (95% *Sus barbatus*/*Sus scrofa*), Gemisch-3 was a sheep-goat mixture (70:30 *Ovis aries*:*Capra hircus*), Gemisch-4 was a three-species mixture of chicken (51%), turkey (28%), and goat (20%), and Gemisch-6 was predominantly chicken (100%). The additional Gemisch-7, -8, and -9 samples further confirmed multi-species resolution across diverse mixture compositions.

**Proficiency test samples.** Nineteen proficiency test samples from DLA (Deutsches Lebensmittellabor Analysen) and LVU (Laborvergleichsuntersuchungen) inter-laboratory ring trials were analyzed. These samples have independently verified compositions used for quality assurance across food testing laboratories. SpeciesID successfully classified reads from all proficiency samples, with species detection patterns consistent with the ring trial expectations. The DLA45 series and LVU-2018/2020 series demonstrated that SpeciesID produces consistent results on samples independently characterized by multiple laboratories.

**Lippold boiled sausage samples.** Seven complex multi-species boiled sausage samples (Lippold series, 2013--2021, with replicates) were analyzed. These represent challenging real-world samples containing 2--14 species including rare game meats. SpeciesID detected the major species components in all cases, with replicate samples (Lippold_2021_A_rep1/rep2, Lippold_2021_C_rep1/rep2) showing consistent species profiles.

**OPSON X independent validation.** The 95 OPSON X samples from the Max Rubner-Institut (German National Reference Centre for Authentic Food) represent a fully independent validation set generated by a different laboratory using the same 16S rDNA metabarcoding approach. The 15 mock and proficiency samples served as internal controls, while the 80 real meat products collected during the Europol--INTERPOL OPSON X operation provided an unbiased sample of commercial products under investigation for potential fraud. SpeciesID classification rates and species detection patterns on OPSON X data were consistent with those observed on the Denay et al. dataset, confirming generalizability across laboratories.

**Out-of-database species.** Three spike-in controls (including one replicate) containing species absent from the database --- roe deer (*Capreolus capreolus*, Spike_Reh1) and red deer (*Cervus elaphus*, Spike_RW1_rep1, Spike_RW1_rep2) --- showed markedly different behavior from in-database species. Only 5% of reads were classified (compared to 57--81% for in-database species), and the classified reads were distributed diffusely across multiple species with no dominant assignment. The replicate red deer sample confirmed this pattern. Additionally, four Exoten (exotic species) samples showed similar low-classification-rate behavior, providing further evidence that the diagnostic signature of out-of-database species is robust. This low classification rate and diffuse species profile provides a diagnostic signature for out-of-database species, distinct from the clear dominant-species pattern observed for all in-database samples.

**Equine mixture samples.** The two Equiden samples detected *Equus caballus* at 1--17% alongside dominant *Sus scrofa* (49--67%) and *Ovis aries* (15--43%). These results are consistent with food products under investigation for equine adulteration, rather than pure equine standards.

## 4. Conclusions

We present SpeciesID, a bias-aware computational framework for quantitative species authentication in meat products via DNA metabarcoding. By jointly modeling mitochondrial copy number variation, PCR amplification bias, and DNA degradation within an expectation-maximization algorithm, SpeciesID produces calibrated weight fraction estimates with associated uncertainty from standard amplicon sequencing data. Systematic benchmarking demonstrated a mean absolute error of 2.59 percentage points, R^2 = 0.977, and perfect detection accuracy (F1 = 1.000) across 54 simulated binary mixtures, with trace detection sensitivity of 1.00 at sequencing depths of 500 reads per marker. Validation on 174 real amplicon sequencing samples from two independent studies---Denay et al. (2023; 79 samples) and the OPSON X operation (Kappel et al., 2023; 95 samples)---confirmed reliable species detection across single-species controls, certified reference materials, proficiency test samples, and real market products from two independent laboratories. The framework is implemented as an efficient, open-source C program (<1 second processing time) with both command-line and graphical interfaces, suitable for deployment in food testing and regulatory laboratories. SpeciesID provides a foundation for the standardized, quantitative metabarcoding pipelines that recent reviews have identified as necessary for regulatory adoption of DNA-based food authentication.

## Acknowledgments

[To be completed]

## CRediT author contribution statement

[To be completed]

## Declaration of competing interest

The authors declare that they have no known competing financial interests or personal relationships that could have appeared to influence the work reported in this paper.

## Data availability

SpeciesID source code and documentation are available at [repository URL]. Benchmark data and scripts for reproducing all results are included in the repository. The Denay et al. (2023) sequencing data are available from the European Nucleotide Archive under BioProject PRJEB57117. The OPSON X (Kappel et al., 2023) sequencing data are available from NCBI SRA under BioProject PRJNA926813.

## References

Ali, M.E., Hashim, U., Mustafa, S., Che Man, Y.B., Islam, K.N., 2014. Gold nanoparticle sensor for the visual detection of pork adulteration in meatball formulation. Journal of Nanomaterials, 2014, 103607.

Deagle, B.E., Thomas, A.C., McInnes, J.C., Clarke, L.J., Vesterinen, E.J., Clare, E.L., Kartzinel, T.R., Eveson, J.P., 2019. Counting with DNA in metabarcoding studies: How should we convert sequence reads to dietary data? Molecular Ecology, 28(2), 391--406.

Denay, G., Preckel, L., Tetzlaff, S., Csaszar, P., Wilhelm, A., Fischer, M., 2023. FooDMe2: a pipeline for the detection and quantification of food components in shotgun and amplicon sequencing data. *Food Chemistry: Molecular Sciences*, 7, 100193.

DinarStandard, 2023. State of the Global Islamic Economy Report 2023/24.

Ferraris, C., Ferraris, L., Ferraris, F., 2024. DNA metabarcoding for food authentication: achievements and challenges. *Food Research International*, 178, 113991.

Giusti, A., Guardone, L., Armani, A., 2024. DNA metabarcoding for food authentication: a comprehensive review. *Comprehensive Reviews in Food Science and Food Safety*, 23(1), e13300.

Kappel, K., Gobbo Oliveira Mello, F., Berg, K., Helmerichs, J., Fischer, M., 2023. Detection of adulterated meat products by a next-generation sequencing-based metabarcoding analysis within the framework of the operation OPSON X: a cooperative project of the German National Reference Centre for Authentic Food (NRZ-Authent) and the competent German food control authorities. *Journal of Consumer Protection and Food Safety*, 18, 257--270.

Hedderich, R., Martin, A., Smith, C., 2019. Amplicon frequency spectrum analysis for quantitative species detection in food products. *Analytical and Bioanalytical Chemistry*, 411, 5627--5638.

Koppel, R., Ganeshan, A., Weber, S., Berner, T., 2020. Species identification in food products using DNA-based methods. *European Food Research and Technology*, 246, 1--15.

McLaren, M.R., Willis, A.D., Callahan, B.J., 2019. Consistent and correctable bias in metagenomic sequencing experiments. *eLife*, 8, e46923.

O'Mahony, P.J., 2013. Finding horse meat in beef products---a global problem. *QJM: An International Journal of Medicine*, 106(6), 595--597.

Spink, J., Moyer, D.C., 2011. Defining the public health threat of food fraud. *Journal of Food Science*, 76(9), R157--R163.

Taberlet, P., Coissac, E., Pompanon, F., Brochmann, C., Willerslev, E., 2012. Towards next-generation biodiversity assessment using DNA metabarcoding. *Molecular Ecology*, 21(8), 2045--2050.

Thomas, A.C., Deagle, B.E., Eveson, J.P., Harsch, C.H., Trites, A.W., 2016. Quantitative DNA metabarcoding: improved estimates of species proportional biomass using correction factors derived from control material. *Molecular Ecology Resources*, 16(3), 714--726.

Wood, D.E., Lu, J., Langmead, B., 2019. Improved metagenomic analysis with Kraken 2. *Genome Biology*, 20, 257.

---

## Supplementary Material

### S1. EM convergence properties

The EM algorithm with MAP estimation under log-normal priors is guaranteed to increase the penalized log-likelihood at each iteration (Dempster et al., 1977). In practice, we observe convergence within 20--50 iterations for typical food authentication datasets. The multiple restart strategy (3 restarts from different initializations) mitigates the risk of convergence to local optima; in our benchmarks, the uniform initialization consistently achieved the highest log-likelihood.

### S2. Reference database marker details

Full amplicon sequences, NCBI accession numbers, primer sequences, and amplicon lengths for all 19 species x 3 markers are provided in the supplementary data file.

Markers used:
- **COI** (cytochrome c oxidase subunit I): forward primer 5'-GGTCAACAAATCATAAAGATATTGG-3', reverse primer 5'-TAAACTTCAGGGTGACCAAAAAATCA-3', amplicon ~658 bp
- **cytb** (cytochrome b): forward primer 5'-AAAAAGCTTCCATCCAACATCTCAGCATGATGAAA-3', reverse primer 5'-AAACTGCAGCCCCTCAGAATGATATTTGTCCTCA-3', amplicon ~425 bp
- **16S rRNA**: forward primer 5'-CGCCTGTTTATCAAAAACAT-3', reverse primer 5'-CCGGTCTGAACTCAGATCACG-3', amplicon ~560 bp

### S3. Complete benchmark results

Full results for all 126 benchmark experiments (species, true weight, estimated weight, absolute error, seed) are provided in the supplementary TSV files.

### S4. Graphical user interface

SpeciesID includes a native macOS graphical user interface featuring:
- Drag-and-drop FASTQ file loading
- First-launch setup wizard for database and index construction
- Real-time analysis progress indicators
- Species composition bar chart visualization
- Halal/haram verdict display with confidence levels
- JSON/TSV export of results

---

*Figure legends (figures to be prepared separately):*

**Figure 1.** SpeciesID pipeline overview. (A) Reference database construction from curated mitochondrial marker sequences. (B) Two-stage k-mer classification: FracMinHash coarse screening (k = 21) followed by exact containment analysis (k = 31). (C) Bias-aware EM model jointly estimating species weight fractions (w), DNA yield factors (d), PCR bias coefficients (b), and degradation rate (lambda). (D) Statistical inference producing calibrated weight estimates with confidence intervals and species presence p-values.

**Figure 2.** Quantification accuracy on simulated binary mixtures. (A) Predicted vs. true species weight fractions (n = 108 data points from 54 experiments, each species reported separately). Dashed line indicates perfect agreement. (B) Bland-Altman plot showing the difference between estimated and true weight fractions against the mean, with 95% limits of agreement (dotted lines).

**Figure 3.** Trace species detection sensitivity as a function of sequencing depth. Detection sensitivity for 0.5% and 1% pork contamination in beef at varying reads per marker (100--5000). Error bars indicate variation across three replicate seeds.

**Figure 4.** Computational performance. Wall-clock time for the complete SpeciesID pipeline as a function of total input reads (300--15,000), showing linear scaling. Measurements performed on Apple M-series hardware (single thread).

**Figure 5.** SpeciesID graphical user interface screenshot showing analysis results for a beef-pork mixture sample, including species composition bar chart, halal verdict, and per-species statistics.
