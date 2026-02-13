#include "fastq.h"
#include "utils.h"
#include <zlib.h>
#include "kseq.h"

KSEQ_INIT(gzFile, gzread)

struct hs_seqfile_s {
    gzFile fp;
    kseq_t *ks;
};

hs_seqfile_t *hs_seqfile_open(const char *path) {
    gzFile fp;
    if (strcmp(path, "-") == 0) {
        fp = gzdopen(0, "r"); /* stdin */
    } else {
        fp = gzopen(path, "r");
    }
    if (!fp) return NULL;
    hs_seqfile_t *sf = (hs_seqfile_t *)hs_calloc(1, sizeof(hs_seqfile_t));
    sf->fp = fp;
    sf->ks = kseq_init(fp);
    return sf;
}

int hs_seqfile_read(hs_seqfile_t *sf, hs_seq_t *rec) {
    int ret = kseq_read(sf->ks);
    if (ret < 0) return -1;
    rec->name = sf->ks->name.s;
    rec->seq = sf->ks->seq.s;
    rec->qual = sf->ks->qual.l > 0 ? sf->ks->qual.s : NULL;
    rec->seq_len = (int)sf->ks->seq.l;
    return 0;
}

void hs_seqfile_close(hs_seqfile_t *sf) {
    if (sf) {
        kseq_destroy(sf->ks);
        gzclose(sf->fp);
        free(sf);
    }
}

int hs_fasta_read_all(const char *path, char ***seqs, char ***names, int **lens, int *n) {
    hs_seqfile_t *sf = hs_seqfile_open(path);
    if (!sf) return -1;

    int cap = 64;
    *n = 0;
    *seqs = (char **)hs_malloc(cap * sizeof(char *));
    *names = (char **)hs_malloc(cap * sizeof(char *));
    *lens = (int *)hs_malloc(cap * sizeof(int));

    hs_seq_t rec;
    while (hs_seqfile_read(sf, &rec) == 0) {
        if (*n >= cap) {
            cap *= 2;
            *seqs = (char **)hs_realloc(*seqs, cap * sizeof(char *));
            *names = (char **)hs_realloc(*names, cap * sizeof(char *));
            *lens = (int *)hs_realloc(*lens, cap * sizeof(int));
        }
        (*seqs)[*n] = hs_strdup(rec.seq);
        (*names)[*n] = hs_strdup(rec.name);
        (*lens)[*n] = rec.seq_len;
        (*n)++;
    }

    hs_seqfile_close(sf);
    return 0;
}

void hs_fasta_free_all(char **seqs, char **names, int *lens, int n) {
    for (int i = 0; i < n; i++) {
        free(seqs[i]);
        free(names[i]);
    }
    free(seqs);
    free(names);
    free(lens);
}
