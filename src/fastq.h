#ifndef SPECIESID_FASTQ_H
#define SPECIESID_FASTQ_H

#include <stdio.h>
#include <zlib.h>

/* Simple FASTQ/FASTA record */
typedef struct {
    char *name;
    char *seq;
    char *qual;    /* NULL for FASTA */
    int seq_len;
} hs_seq_t;

/* FASTQ/FASTA reader (wraps kseq.h) */
typedef struct hs_seqfile_s hs_seqfile_t;

hs_seqfile_t *hs_seqfile_open(const char *path);
int hs_seqfile_read(hs_seqfile_t *sf, hs_seq_t *rec);  /* 0 = success, -1 = EOF */
void hs_seqfile_close(hs_seqfile_t *sf);

/* Read all sequences from a FASTA file into arrays */
int hs_fasta_read_all(const char *path, char ***seqs, char ***names, int **lens, int *n);
void hs_fasta_free_all(char **seqs, char **names, int *lens, int n);

#endif /* SPECIESID_FASTQ_H */
