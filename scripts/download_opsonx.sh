#!/bin/bash
# Download OPSON X amplicon sequencing data (Kappel et al. 2023)
# BioProject: PRJNA926813
# Convenience wrapper around download_real_data.sh
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== OPSON X Dataset Download ==="
echo "Kappel et al. (2023) - Journal of Consumer Protection and Food Safety"
echo "BioProject: PRJNA926813"
echo "95 samples: 10 mock + 5 proficiency + 80 real meat products"
echo ""

# Test with one sample first if --test flag is given
if [[ "${1:-}" == "--test" ]]; then
    echo "=== TEST MODE: downloading one sample to check primer compatibility ==="
    FASTERQ="/opt/homebrew/bin/fasterq-dump"
    OUTDIR="data/benchmark/opsonx"
    mkdir -p "$OUTDIR"

    TEST_ACC="SRR23225544"  # OPSONX_001 (mock mixture)
    TEST_FILE="${OUTDIR}/${TEST_ACC}_1.fastq"

    if [[ ! -f "$TEST_FILE" ]]; then
        echo "Downloading ${TEST_ACC} (OPSONX_001)..."
        "$FASTERQ" --split-3 --outdir "$OUTDIR" "$TEST_ACC" 2>/dev/null
        if [[ -f "${OUTDIR}/${TEST_ACC}_2.fastq" ]]; then
            rm -f "${OUTDIR}/${TEST_ACC}_2.fastq"
        fi
        if [[ -f "${OUTDIR}/${TEST_ACC}.fastq" ]] && [[ ! -f "$TEST_FILE" ]]; then
            mv "${OUTDIR}/${TEST_ACC}.fastq" "$TEST_FILE"
        fi
    fi

    if [[ -f "$TEST_FILE" ]]; then
        n_reads=$(( $(wc -l < "$TEST_FILE") / 4 ))
        echo "Downloaded: ${n_reads} reads"
        echo ""
        echo "Run SpeciesID to check classification rate:"
        echo "  ./halalseq run -x halal.idx -r $TEST_FILE -o /tmp/opsonx_test.json -f json --prune 0.05"
        echo ""
        echo "If classification rate >30%, the primers are compatible. Proceed with full download:"
        echo "  bash scripts/download_real_data.sh opsonx"
    else
        echo "ERROR: download failed"
        exit 1
    fi
    exit 0
fi

# Full download
bash "${SCRIPT_DIR}/download_real_data.sh" opsonx
