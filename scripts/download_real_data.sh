#!/bin/bash
# Download real amplicon sequencing data for SpeciesID validation
# Supports multiple datasets: Denay et al. (2023) and OPSON X (Kappel et al. 2023)
# Uses fasterq-dump from SRA Toolkit
set -eo pipefail

FASTERQ="/opt/homebrew/bin/fasterq-dump"
BASE_DATADIR="data/benchmark"

usage() {
    echo "Usage: $0 [--dataset denay|opsonx|all]"
    echo ""
    echo "Datasets:"
    echo "  denay   - Denay et al. (2023), BioProject PRJEB57117 (79 samples)"
    echo "  opsonx  - Kappel et al. (2023), BioProject PRJNA926813 (95 samples)"
    echo "  all     - Both datasets (default)"
    exit 1
}

download_from_ground_truth() {
    local gt_file="$1"
    local outdir="$2"
    local dataset_name="$3"

    if [[ ! -f "$gt_file" ]]; then
        echo "Error: ground truth file $gt_file not found"
        return 1
    fi

    mkdir -p "$outdir"

    local downloaded=0
    local skipped=0
    local failed=0
    local total=0

    total=$(tail -n +2 "$gt_file" | wc -l | tr -d ' ')

    echo "=== Downloading ${dataset_name} (${total} samples) ==="
    echo "Ground truth: $gt_file"
    echo "Output dir:   $outdir"
    echo ""

    tail -n +2 "$gt_file" | while IFS=$'\t' read -r accession sample_name category species_expected notes; do
        [ -z "$accession" ] && continue
        outfile="${outdir}/${accession}_1.fastq"

        if [ -f "$outfile" ]; then
            echo "[SKIP] ${accession} (${sample_name}) -- already exists"
            skipped=$((skipped + 1))
            continue
        fi

        echo -n "[DOWN] ${accession} (${sample_name}) ... "
        if "$FASTERQ" --split-3 --outdir "$outdir" "$accession" 2>/dev/null; then
            # fasterq-dump creates files like ERR10436160_1.fastq, ERR10436160_2.fastq
            # We only need R1 for single-end/amplicon analysis
            if [ -f "${outdir}/${accession}_2.fastq" ]; then
                rm -f "${outdir}/${accession}_2.fastq"
            fi
            # Some runs produce a single file (not _1/_2)
            if [ -f "${outdir}/${accession}.fastq" ] && [ ! -f "$outfile" ]; then
                mv "${outdir}/${accession}.fastq" "$outfile"
            fi
            if [ -f "$outfile" ]; then
                n_reads=$(( $(wc -l < "$outfile") / 4 ))
                echo "OK (${n_reads} reads)"
                downloaded=$((downloaded + 1))
            else
                echo "WARN: no R1 file produced"
                failed=$((failed + 1))
            fi
        else
            echo "FAILED"
            failed=$((failed + 1))
        fi
    done

    echo ""
    echo "=== ${dataset_name} download complete ==="
    echo "FASTQ files in ${outdir}:"
    ls -1 "${outdir}"/*.fastq 2>/dev/null | wc -l | tr -d ' '
    echo ""
}

# Parse arguments
DATASET="all"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dataset)
            DATASET="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            DATASET="$1"
            shift
            ;;
    esac
done

case "$DATASET" in
    denay)
        download_from_ground_truth \
            "${BASE_DATADIR}/ground_truth_denay_full.tsv" \
            "${BASE_DATADIR}" \
            "Denay et al. (2023) - PRJEB57117"
        ;;
    opsonx)
        download_from_ground_truth \
            "${BASE_DATADIR}/ground_truth_opsonx.tsv" \
            "${BASE_DATADIR}/opsonx" \
            "OPSON X (Kappel et al. 2023) - PRJNA926813"
        ;;
    all)
        download_from_ground_truth \
            "${BASE_DATADIR}/ground_truth_denay_full.tsv" \
            "${BASE_DATADIR}" \
            "Denay et al. (2023) - PRJEB57117"
        download_from_ground_truth \
            "${BASE_DATADIR}/ground_truth_opsonx.tsv" \
            "${BASE_DATADIR}/opsonx" \
            "OPSON X (Kappel et al. 2023) - PRJNA926813"
        ;;
    *)
        echo "Error: unknown dataset '$DATASET'"
        usage
        ;;
esac
