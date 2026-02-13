#!/bin/bash
# Enumerate all run accessions in PRJEB57117 (Denay et al. 2023)
# Queries the ENA API to list all available runs with sample metadata
set -eo pipefail

echo "=== Enumerating PRJEB57117 accessions ==="
echo ""

OUTFILE="${1:-data/benchmark/denay_accessions.tsv}"
mkdir -p "$(dirname "$OUTFILE")"

curl -s "https://www.ebi.ac.uk/ena/portal/api/filereport?accession=PRJEB57117&result=read_run&fields=run_accession,sample_alias,experiment_title&format=tsv" \
    > "$OUTFILE"

n_runs=$(tail -n +2 "$OUTFILE" | wc -l | tr -d ' ')
echo "Found ${n_runs} runs in PRJEB57117"
echo "Output: $OUTFILE"
echo ""
echo "Sample aliases:"
tail -n +2 "$OUTFILE" | awk -F'\t' '{print "  " $1 "\t" $2}'
