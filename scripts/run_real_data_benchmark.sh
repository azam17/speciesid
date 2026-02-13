#!/bin/bash
# Run SpeciesID on real data samples from multiple datasets
# Reads ground_truth TSV, runs each sample, outputs results TSV
#
# Usage:
#   bash scripts/run_real_data_benchmark.sh [denay|opsonx|all]
set -euo pipefail

SPECIESID="./halalseq"
IDX="halal.idx"
OUTDIR="benchmark_results"

mkdir -p "$OUTDIR"

if [[ ! -f "$IDX" ]]; then
    echo "Error: index file $IDX not found. Run: $SPECIESID build-db -o halal.db && $SPECIESID index -d halal.db -o halal.idx"
    exit 1
fi

run_benchmark() {
    local gt_file="$1"
    local datadir="$2"
    local dataset_tag="$3"
    local dataset_label="$4"

    if [[ ! -f "$gt_file" ]]; then
        echo "Error: ground truth file $gt_file not found"
        return 1
    fi

    # Output files namespaced by dataset
    local results_tsv="${OUTDIR}/real_data_${dataset_tag}.tsv"
    local details_tsv="${OUTDIR}/real_data_${dataset_tag}_details.tsv"

    echo -e "accession\tsample_name\tcategory\tspecies\tweight_pct\tread_pct\tp_value\texpected\tis_expected\ttotal_reads\tclassified_reads" > "$results_tsv"
    echo -e "accession\tsample_name\tcategory\ttotal_reads\tclassified_reads\tclassified_pct\tn_species_detected\tverdict\texpected_species" > "$details_tsv"

    echo "=== SpeciesID Real Data Benchmark: ${dataset_label} ==="
    echo "Ground truth: $gt_file"
    echo "Data dir:     $datadir"
    echo ""

    local n_processed=0
    local n_skipped=0

    # Read ground truth (skip header)
    tail -n +2 "$gt_file" | while IFS=$'\t' read -r accession sample_name category species_expected notes; do
        fastq="${datadir}/${accession}_1.fastq"
        json_out="/tmp/speciesid_real_${accession}.json"

        if [[ ! -f "$fastq" ]]; then
            echo "[SKIP] ${accession} (${sample_name}) -- FASTQ not found"
            n_skipped=$((n_skipped + 1))
            continue
        fi

        echo -n "[RUN]  ${accession} (${sample_name}, ${category}) ... "

        # Run SpeciesID
        $SPECIESID run -x "$IDX" -r "$fastq" -o "$json_out" -f json --prune 0.05 2>/dev/null

        if [[ ! -f "$json_out" ]]; then
            echo "FAILED (no output)"
            continue
        fi

        # Parse JSON and produce TSV lines
        python3 -c "
import json, sys

with open('${json_out}') as f:
    data = json.load(f)

accession = '${accession}'
sample_name = '${sample_name}'
category = '${category}'
species_expected = '${species_expected}'

total_reads = data.get('total_reads', 0)
classified_reads = data.get('classified_reads', 0)
classified_pct = (classified_reads / total_reads * 100) if total_reads > 0 else 0
verdict = data.get('verdict', 'UNKNOWN')

expected_set = set()
if species_expected not in ('unknown', 'mixture', 'mixture_certified', 'exotic_species'):
    expected_set = set(species_expected.split(','))

detected_species = []
for sp_data in data.get('species', []):
    sp = sp_data['species']
    w = sp_data['weight_pct']
    rp = sp_data['read_pct']
    pv = sp_data['p_value']
    is_exp = 'yes' if sp in expected_set else ('na' if not expected_set else 'no')
    detected_species.append(sp)
    # Write to results TSV
    print(f'{accession}\t{sample_name}\t{category}\t{sp}\t{w:.4f}\t{rp:.4f}\t{pv}\t{species_expected}\t{is_exp}\t{total_reads}\t{classified_reads}')

# Write summary to details TSV (to stderr for separate capture)
n_det = len(detected_species)
sys.stderr.write(f'{accession}\t{sample_name}\t{category}\t{total_reads}\t{classified_reads}\t{classified_pct:.1f}\t{n_det}\t{verdict}\t{species_expected}\n')
" >> "$results_tsv" 2>> "$details_tsv"

        # Print summary
        n_species=$(python3 -c "
import json
with open('${json_out}') as f:
    data = json.load(f)
cr = data.get('classified_reads', 0)
tr = data.get('total_reads', 0)
ns = len(data.get('species', []))
top = data['species'][0]['species'] if data.get('species') else 'none'
tw = data['species'][0]['weight_pct'] if data.get('species') else 0
print(f'{cr}/{tr} classified, {ns} species, top: {top} ({tw:.1f}%)')
")
        echo "$n_species"

        rm -f "$json_out"
        n_processed=$((n_processed + 1))
    done

    echo ""
    echo "=== ${dataset_label} benchmark complete ==="
    echo "Results: $results_tsv"
    echo "Details: $details_tsv"
    echo ""

    # Quick summary
    if [[ -f "$results_tsv" ]]; then
        n_samples=$(tail -n +2 "$details_tsv" | wc -l | tr -d ' ')
        echo "Samples processed: ${n_samples}"
        echo ""

        # Compute detection metrics for spike samples
        python3 -c "
import csv
from collections import defaultdict

results = []
with open('${results_tsv}') as f:
    reader = csv.DictReader(f, delimiter='\t')
    for row in reader:
        results.append(row)

# Spike samples with known ground truth
spike_results = [r for r in results if r['category'] == 'spike']
if spike_results:
    tp = sum(1 for r in spike_results if r['is_expected'] == 'yes')
    fp = sum(1 for r in spike_results if r['is_expected'] == 'no')
    # Count expected species not detected
    by_sample = defaultdict(list)
    for r in spike_results:
        by_sample[r['accession']].append(r)
    fn = 0
    for acc, rows in by_sample.items():
        expected = rows[0]['expected'].split(',') if rows[0]['expected'] not in ('unknown', 'mixture') else []
        detected = set(r['species'] for r in rows)
        fn += sum(1 for e in expected if e not in detected)

    print(f'Spike samples (known single species):')
    print(f'  TP={tp}, FP={fp}, FN={fn}')
    if (tp + fn) > 0:
        sens = tp / (tp + fn)
        print(f'  Sensitivity: {sens:.3f}')
    if (tp + fp) > 0:
        prec = tp / (tp + fp)
        print(f'  Precision: {prec:.3f}')
"
    fi
}

# Parse arguments
DATASET="${1:-all}"

case "$DATASET" in
    denay)
        run_benchmark \
            "data/benchmark/ground_truth_denay_full.tsv" \
            "data/benchmark" \
            "denay" \
            "Denay et al. (2023) - PRJEB57117"
        ;;
    opsonx)
        run_benchmark \
            "data/benchmark/ground_truth_opsonx.tsv" \
            "data/benchmark/opsonx" \
            "opsonx" \
            "OPSON X (Kappel et al. 2023) - PRJNA926813"
        ;;
    all)
        run_benchmark \
            "data/benchmark/ground_truth_denay_full.tsv" \
            "data/benchmark" \
            "denay" \
            "Denay et al. (2023) - PRJEB57117"
        run_benchmark \
            "data/benchmark/ground_truth_opsonx.tsv" \
            "data/benchmark/opsonx" \
            "opsonx" \
            "OPSON X (Kappel et al. 2023) - PRJNA926813"
        ;;
    *)
        echo "Error: unknown dataset '$DATASET'. Use: denay, opsonx, or all"
        exit 1
        ;;
esac

# If running all, also create combined summary
if [[ "$DATASET" == "all" ]]; then
    echo ""
    echo "============================================================"
    echo "  Combined Multi-Dataset Summary"
    echo "============================================================"

    python3 -c "
import csv, os
from collections import defaultdict

results_dir = '${OUTDIR}'
datasets = {
    'denay': 'Denay et al. (2023)',
    'opsonx': 'OPSON X (Kappel et al. 2023)',
}

total_samples = 0
total_reads = 0
total_classified = 0

for tag, label in datasets.items():
    details_file = os.path.join(results_dir, f'real_data_{tag}_details.tsv')
    if not os.path.exists(details_file):
        continue
    with open(details_file) as f:
        reader = csv.DictReader(f, delimiter='\t')
        rows = list(reader)
    n = len(rows)
    tr = sum(int(r.get('total_reads', 0)) for r in rows)
    cr = sum(int(r.get('classified_reads', 0)) for r in rows)
    pct = cr / tr * 100 if tr > 0 else 0
    total_samples += n
    total_reads += tr
    total_classified += cr
    print(f'  {label}: {n} samples, {tr:,} reads, {cr:,} classified ({pct:.1f}%)')

if total_reads > 0:
    overall_pct = total_classified / total_reads * 100
    print(f'')
    print(f'  TOTAL: {total_samples} samples, {total_reads:,} reads, {total_classified:,} classified ({overall_pct:.1f}%)')
"
fi
