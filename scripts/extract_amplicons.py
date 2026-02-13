#!/usr/bin/env python3
"""
Download NCBI mitochondrial genomes and extract amplicon regions for SpeciesID.

Outputs:
  - data/mitogenomes/<accession>.fa   (downloaded full mitogenomes)
  - data/amplicons/<Species>_<marker>.fa  (extracted amplicon regions)
  - src/refseqs.h                       (C header with embedded sequences)

Usage:
  python3 scripts/extract_amplicons.py
"""

import os
import sys
import time
import urllib.request
import re

# ---------- Configuration ----------

SPECIES = [
    # (species_id, accession, description)
    ("Bos_taurus",          "NC_006853", "Cattle"),
    ("Sus_scrofa",          "NC_000845", "Domestic pig"),
    ("Ovis_aries",          "NC_001941", "Sheep"),
    ("Capra_hircus",        "NC_005044", "Goat"),
    ("Gallus_gallus",       "NC_001323", "Chicken"),
    ("Bubalus_bubalis",     "NC_006295", "Water buffalo"),
    ("Equus_caballus",      "NC_001640", "Horse"),
    ("Equus_asinus",        "NC_001788", "Donkey"),
    ("Meleagris_gallopavo", "NC_010195", "Turkey"),
    ("Anas_platyrhynchos",  "NC_009684", "Duck"),
    ("Canis_lupus",         "NC_002008", "Dog"),
    ("Sus_barbatus",        "NC_026992", "Bearded pig"),
    # Fish species (Halal concern for cross-contamination)
    ("Salmo_salar",         "NC_001960", "Atlantic salmon"),
    ("Oreochromis_niloticus", "NC_013663", "Nile tilapia"),
    ("Gadus_morhua",        "NC_002081", "Atlantic cod"),
    ("Pangasianodon_hypophthalmus", "NC_023966", "Pangasius"),
    # Plant fillers (Adulterant detection)
    ("Glycine_max",         "NC_007942", "Soybean (mitogenome)"),
    ("Triticum_aestivum",   "NC_036024", "Bread wheat (mitogenome)"),
    ("Oryza_sativa",        "NC_011033", "Rice (mitogenome)"),
]

# Primer pairs for each marker
MARKERS = {
    "COI": {
        "forward": "GGTCAACAAATCATAAAGATATTGG",   # LCO1490
        "reverse": "TAAACTTCAGGGTGACCAAAAAATCA",  # HCO2198
    },
    "cytb": {
        "forward": "CCATCCAACATCTCAGCATGATG",      # L14724
        "reverse": "CCCCTCAGAATGATATTTGTCCTCA",   # H15149
    },
    "16S": {
        "forward": "GACGAGAAGACCCTATGGAGC",         # Tillmar 2013 forward
        "reverse": "TCCGAGGTCGCCCCAACC",            # Tillmar 2013 reverse (R→G resolved)
    },
}

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MITOGENOME_DIR = os.path.join(BASE_DIR, "data", "mitogenomes")
AMPLICON_DIR = os.path.join(BASE_DIR, "data", "amplicons")
REFSEQS_H = os.path.join(BASE_DIR, "src", "refseqs.h")


def download_fasta(accession, outpath):
    """Download a FASTA sequence from NCBI Entrez."""
    if os.path.exists(outpath):
        print(f"  [cached] {outpath}")
        return True
    url = (
        f"https://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi"
        f"?db=nucleotide&id={accession}&rettype=fasta&retmode=text"
    )
    print(f"  Downloading {accession} ...")
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "SpeciesID/1.0"})
        with urllib.request.urlopen(req, timeout=30) as resp:
            data = resp.read().decode("utf-8")
        if not data.startswith(">"):
            print(f"  ERROR: unexpected response for {accession}")
            return False
        with open(outpath, "w") as f:
            f.write(data)
        time.sleep(0.4)  # NCBI rate limit: 3 requests/sec
        return True
    except Exception as e:
        print(f"  ERROR downloading {accession}: {e}")
        return False


def read_fasta(path):
    """Read a single-sequence FASTA file, return uppercase sequence string."""
    seq_parts = []
    with open(path) as f:
        for line in f:
            if line.startswith(">"):
                continue
            seq_parts.append(line.strip().upper())
    return "".join(seq_parts)


def revcomp(seq):
    """Reverse complement of a DNA sequence."""
    comp = str.maketrans("ACGTRYSWKMBDHVN", "TGCAYRSWMKVHDBN")
    return seq.translate(comp)[::-1]


def expand_iupac(base):
    """Expand IUPAC ambiguity code to list of possible bases."""
    codes = {
        'A': ['A'], 'C': ['C'], 'G': ['G'], 'T': ['T'],
        'R': ['A', 'G'], 'Y': ['C', 'T'], 'S': ['G', 'C'],
        'W': ['A', 'T'], 'K': ['G', 'T'], 'M': ['A', 'C'],
        'B': ['C', 'G', 'T'], 'D': ['A', 'G', 'T'],
        'H': ['A', 'C', 'T'], 'V': ['A', 'C', 'G'],
        'N': ['A', 'C', 'G', 'T'],
    }
    return codes.get(base.upper(), [base.upper()])


def primer_match(seq, primer, max_mismatches=2):
    """
    Search for primer binding site in seq, allowing up to max_mismatches.
    Returns list of (start, end, mismatches) tuples for all matches.
    """
    plen = len(primer)
    matches = []
    for i in range(len(seq) - plen + 1):
        mm = 0
        for j in range(plen):
            if seq[i + j] not in expand_iupac(primer[j]):
                mm += 1
                if mm > max_mismatches:
                    break
        if mm <= max_mismatches:
            matches.append((i, i + plen, mm))
    return matches


def find_amplicon(genome, fwd_primer, rev_primer, max_mismatches=2,
                  min_len=100, max_len=2000):
    """
    Find amplicon region in genome between forward and reverse primers.
    Searches both strands. Returns (sequence, start, end) or None.
    """
    # Search forward strand
    fwd_hits = primer_match(genome, fwd_primer, max_mismatches)
    # Reverse primer binds on reverse strand, so search for its revcomp on fwd strand
    rev_rc = revcomp(rev_primer)
    rev_hits = primer_match(genome, rev_rc, max_mismatches)

    best = None
    best_mm = 999
    for fs, fe, fmm in fwd_hits:
        for rs, re_, rmm in rev_hits:
            amp_len = re_ - fs
            if min_len <= amp_len <= max_len:
                total_mm = fmm + rmm
                if total_mm < best_mm:
                    best = (genome[fs:re_], fs, re_)
                    best_mm = total_mm

    if best:
        return best

    # Try reverse complement of genome (for genes on minus strand)
    rc_genome = revcomp(genome)
    fwd_hits = primer_match(rc_genome, fwd_primer, max_mismatches)
    rev_hits = primer_match(rc_genome, rev_rc, max_mismatches)

    for fs, fe, fmm in fwd_hits:
        for rs, re_, rmm in rev_hits:
            amp_len = re_ - fs
            if min_len <= amp_len <= max_len:
                total_mm = fmm + rmm
                if total_mm < best_mm:
                    best = (rc_genome[fs:re_], fs, re_)
                    best_mm = total_mm

    return best


def generate_refseqs_h(amplicons):
    """Generate src/refseqs.h with embedded real sequences."""
    lines = []
    lines.append("/* Real mitochondrial amplicon sequences from NCBI RefSeq */")
    lines.append("/* Auto-generated by scripts/extract_amplicons.py -- DO NOT EDIT */")
    lines.append("")
    lines.append("#ifndef SPECIESID_REFSEQS_H")
    lines.append("#define SPECIESID_REFSEQS_H")
    lines.append("")

    # Group by species
    species_order = [sp[0] for sp in SPECIES]
    marker_order = ["COI", "cytb", "16S"]

    for sp_id in species_order:
        for marker in marker_order:
            key = (sp_id, marker)
            if key in amplicons:
                seq, length = amplicons[key]
                var_name = f"SEQ_{sp_id.upper()}_{marker.upper()}"
                # Split sequence into 80-char lines for readability
                chunks = [seq[i:i+76] for i in range(0, len(seq), 76)]
                if len(chunks) == 1:
                    lines.append(f'static const char {var_name}[] = "{chunks[0]}";  /* {length} bp */')
                else:
                    lines.append(f"static const char {var_name}[] =  /* {length} bp */")
                    for ci, chunk in enumerate(chunks):
                        if ci < len(chunks) - 1:
                            lines.append(f'    "{chunk}"')
                        else:
                            lines.append(f'    "{chunk}";')
                lines.append("")

    # Generate lookup table
    lines.append("/* Lookup table: [species_idx][marker_idx] -> sequence pointer and length */")
    lines.append("typedef struct { const char *seq; int len; } refseq_entry_t;")
    lines.append("")

    # species indices match order in refdb_build_default()
    default_species_order = [
        "Sus_scrofa", "Sus_barbatus", "Canis_lupus", "Bos_taurus",
        "Bubalus_bubalis", "Ovis_aries", "Capra_hircus", "Gallus_gallus",
        "Meleagris_gallopavo", "Anas_platyrhynchos", "Equus_caballus", "Equus_asinus",
        "Salmo_salar", "Oreochromis_niloticus", "Gadus_morhua",
        "Pangasianodon_hypophthalmus", "Glycine_max", "Triticum_aestivum",
        "Oryza_sativa",
    ]

    lines.append(f"static const refseq_entry_t REFSEQ_TABLE[{len(default_species_order)}][3] = {{")
    for si, sp_id in enumerate(default_species_order):
        entries = []
        for mi, marker in enumerate(marker_order):
            key = (sp_id, marker)
            if key in amplicons:
                var_name = f"SEQ_{sp_id.upper()}_{marker.upper()}"
                seq, length = amplicons[key]
                entries.append(f"{{ {var_name}, {length} }}")
            else:
                entries.append("{ NULL, 0 }")
        sp_comment = sp_id.replace("_", " ")
        lines.append(f"    /* [{si}] {sp_comment} */ {{ {', '.join(entries)} }},")
    lines.append("};")
    lines.append("")
    lines.append(f"#define REFSEQ_N_SPECIES {len(default_species_order)}")
    lines.append(f"#define REFSEQ_N_MARKERS 3")
    lines.append("")
    lines.append("#endif /* SPECIESID_REFSEQS_H */")
    lines.append("")

    with open(REFSEQS_H, "w") as f:
        f.write("\n".join(lines))
    print(f"\nGenerated {REFSEQS_H}")


def main():
    os.makedirs(MITOGENOME_DIR, exist_ok=True)
    os.makedirs(AMPLICON_DIR, exist_ok=True)

    # Step 1: Download mitogenomes
    print("=== Step 1: Downloading mitogenomes from NCBI ===")
    genomes = {}
    for sp_id, accession, desc in SPECIES:
        fasta_path = os.path.join(MITOGENOME_DIR, f"{accession}.fa")
        if download_fasta(accession, fasta_path):
            genomes[sp_id] = read_fasta(fasta_path)
            print(f"  {sp_id}: {len(genomes[sp_id])} bp")
        else:
            print(f"  SKIP {sp_id}: download failed")

    # Step 2: Extract amplicons
    print("\n=== Step 2: Extracting amplicon regions ===")
    amplicons = {}  # (species_id, marker) -> (sequence, length)

    # Per-marker mismatch tolerance: universal primers need more slack for COI/cytb
    marker_max_mm = {"COI": 5, "cytb": 4, "16S": 3}

    for sp_id in genomes:
        genome = genomes[sp_id]
        for marker, primers in MARKERS.items():
            result = find_amplicon(
                genome,
                primers["forward"],
                primers["reverse"],
                max_mismatches=marker_max_mm[marker],
                min_len=80,
                max_len=2000,
            )
            if result:
                seq, start, end = result
                amplicons[(sp_id, marker)] = (seq, len(seq))
                # Save individual FASTA
                amp_path = os.path.join(AMPLICON_DIR, f"{sp_id}_{marker}.fa")
                with open(amp_path, "w") as f:
                    f.write(f">{sp_id}_{marker} {start}-{end} {len(seq)}bp\n")
                    for i in range(0, len(seq), 80):
                        f.write(seq[i:i+80] + "\n")
                print(f"  {sp_id:24s} {marker:5s}: {len(seq):4d} bp  (pos {start}-{end})")
            else:
                print(f"  {sp_id:24s} {marker:5s}: NOT FOUND")

    # Step 3: Generate C header
    print(f"\n=== Step 3: Generating {REFSEQS_H} ===")
    generate_refseqs_h(amplicons)

    # Summary
    print("\n=== Summary ===")
    total = 0
    for sp_id, _, desc in SPECIES:
        markers_found = [m for m in ["COI", "cytb", "16S"] if (sp_id, m) in amplicons]
        if markers_found:
            print(f"  {sp_id:24s}: {', '.join(markers_found)}")
            total += len(markers_found)
        else:
            print(f"  {sp_id:24s}: (none)")
    print(f"\nTotal: {total} amplicon sequences extracted")


if __name__ == "__main__":
    main()
