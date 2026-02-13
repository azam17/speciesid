/*
 * gui_analysis.h — Background analysis pipeline for SpeciesID GUI.
 *
 * Runs index loading, FASTQ reading, classification, EM, and report
 * generation on a background thread.  The main (GUI) thread polls the
 * state/progress fields to update the UI.
 */
#ifndef SPECIESID_GUI_ANALYSIS_H
#define SPECIESID_GUI_ANALYSIS_H

#include "report.h"
#include <SDL.h>

typedef enum {
    ANALYSIS_IDLE,
    ANALYSIS_LOADING_INDEX,
    ANALYSIS_READING_FASTQ,
    ANALYSIS_CLASSIFYING,
    ANALYSIS_RUNNING_EM,
    ANALYSIS_GENERATING_REPORT,
    ANALYSIS_DONE,
    ANALYSIS_ERROR
} analysis_state_t;

/* Memory estimate for file set */
typedef struct {
    long total_file_bytes;
    long estimated_uncompressed;
    int  estimated_reads;
    int  estimated_ram_mb;
} mem_estimate_t;

/* Sample grouping: one or two files that form a single sample */
typedef struct {
    char sample_name[256];          /* display name, e.g. "SRR23225457" */
    int  file_indices[2];           /* indices into fastq_paths[]       */
    int  n_files;                   /* 1 = single, 2 = paired R1+R2    */
} sample_group_t;

typedef struct {
    /* ---- inputs (set by GUI thread before analysis_start) ---- */
    char fastq_paths[32][1024];       /* up to 32 input files              */
    int  n_fastq_files;
    char index_path[1024];

    /* ---- subsampling ---- */
    int  subsample_enabled;           /* 0 = off, 1 = on                   */
    int  subsample_max_reads;         /* default 500000                    */

    /* ---- sample grouping (computed after file selection) ---- */
    sample_group_t samples[32];
    int  n_samples;

    /* ---- observable state (read by GUI thread, written by worker) ---- */
    volatile analysis_state_t state;
    volatile int progress_reads;      /* reads processed so far            */
    volatile int progress_total;      /* total reads (0 until known)       */
    volatile int progress_file_idx;   /* which file being read (0-based)   */
    volatile int progress_sample_idx; /* which sample being processed      */
    char error_msg[256];

    /* ---- output: one report per sample ---- */
    halal_report_t **reports;         /* [n_samples], NULL until done      */
    int  n_reports;

    /* ---- GUI selection ---- */
    int  selected_sample;             /* which report is displayed         */

    /* ---- internal ---- */
    SDL_Thread *thread;
} analysis_context_t;

/* Initialise to all zeros / ANALYSIS_IDLE. */
void analysis_init(analysis_context_t *ctx);

/* Detect sample groupings from the current file list.
   Pairs R1/R2 files into single samples, singles stay separate.
   Call after modifying fastq_paths[]/n_fastq_files. */
void detect_samples(analysis_context_t *ctx);

/* Launch the background worker.  ctx->fastq_paths and ctx->index_path
   must already be set.  Returns 0 on success, -1 on error. */
int  analysis_start(analysis_context_t *ctx);

/* Estimate memory usage for the current file set. */
mem_estimate_t analysis_estimate_memory(const analysis_context_t *ctx);

/* Free any resources held by a previous run (report, thread handle).
   Safe to call multiple times or on a freshly-init'd context. */
void analysis_cleanup(analysis_context_t *ctx);

/* State label strings for the UI status line. */
const char *analysis_state_label(analysis_state_t s);

#endif /* SPECIESID_GUI_ANALYSIS_H */
