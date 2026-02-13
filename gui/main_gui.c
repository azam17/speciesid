/*
 * main_gui.c — SpeciesID desktop GUI application.
 *
 * SDL2 + Nuklear immediate-mode GUI.  Single window (960x640), left
 * panel for file selection / progress, right panel for results or
 * database viewer.
 *
 * Build:  see Makefile "gui" target or CMakeLists.txt.
 */

/* ================================================================== */
/* Nuklear implementation (must come first, once per translation unit) */
/* ================================================================== */
#include "nuklear_setup.h"

/* ================================================================== */
/* Project headers                                                     */
/* ================================================================== */
#include "gui_analysis.h"
#include "tinyfiledialogs.h"
#include "refdb.h"
#include "index.h"
#include "kmer.h"
#include "update.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <math.h>

#ifdef _WIN32
  #include <direct.h>
  #define SID_MKDIR(p) _mkdir(p)
#else
  #define SID_MKDIR(p) mkdir(p, 0755)
#endif

/* ================================================================== */
/* Constants                                                           */
/* ================================================================== */
#define WINDOW_W  960
#define WINDOW_H  640
#define LEFT_W    340
#define VERSION   "0.2.0"

/* Row heights (logical points) */
#define ROW_TITLE  30
#define ROW_LABEL  26
#define ROW_BTN    38
#define ROW_SMALL  22
#define ROW_TABLE  24
#define ROW_SPACE  8
#define ROW_BAR    20

/* ================================================================== */
/* GUI state bundle                                                    */
/* ================================================================== */
typedef struct {
    analysis_context_t analysis;
    char index_path[1024];
    int  right_panel_mode;    /* 0 = Results, 1 = Database */
    halal_refdb_t *db_info;   /* loaded at startup for database viewer */
    float min_display_pct;    /* minimum % to show in chart, default 0.1 */
    /* First-launch wizard */
    int  show_wizard;         /* 1 = wizard visible, 0 = normal UI */
    int  wizard_step;         /* 0=welcome, 1=index, 2=formats, 3=ready */
    int  index_found;         /* auto-detected on launch */
    volatile int wizard_building; /* 1 = index build in progress */
    Uint32 wizard_start_ticks;    /* SDL ticks when wizard opened (animation) */
    /* Database auto-update */
    sid_update_ctx_t  update_ctx;
    volatile int     update_checking;     /* 1 = check thread running   */
    volatile int     update_available;    /* 1 = newer version exists   */
    volatile int     update_in_progress;  /* 1 = download thread running*/
    int              update_dismissed;    /* 1 = user dismissed banner  */
} gui_state_t;

/* ================================================================== */
/* Colour helpers                                                      */
/* ================================================================== */
static struct nk_color col_pass       = {34, 139, 34, 255};   /* forest green */
static struct nk_color col_fail       = {220, 20, 60, 255};   /* crimson      */
static struct nk_color col_inconc     = {218, 165, 32, 255};  /* goldenrod    */
static struct nk_color col_halal      = {34, 139, 34, 255};
static struct nk_color col_haram      = {220, 20, 60, 255};
static struct nk_color col_mashbooh   = {218, 165, 32, 255};
static struct nk_color col_unknown    = {160, 160, 160, 255};

static struct nk_color status_color(halal_status_t s) {
    switch (s) {
        case HALAL:            return col_halal;
        case HARAM:            return col_haram;
        case MASHBOOH:         return col_mashbooh;
        default:               return col_unknown;
    }
}

static struct nk_color verdict_color(verdict_t v) {
    switch (v) {
        case PASS:         return col_pass;
        case FAIL:         return col_fail;
        default:           return col_inconc;
    }
}

/* ================================================================== */
/* Friendly species name lookup                                        */
/* ================================================================== */
static const char *friendly_species_name(const char *species_id) {
    /* Map Latin names to common food names */
    struct { const char *latin; const char *common; } names[] = {
        { "Bos_taurus",       "Beef (Cow)"           },
        { "Sus_scrofa",       "Pork (Pig)"           },
        { "Ovis_aries",       "Lamb (Sheep)"         },
        { "Gallus_gallus",    "Chicken"              },
        { "Capra_hircus",     "Goat"                 },
        { "Equus_caballus",   "Horse"                },
        { "Bubalus_bubalis",  "Buffalo"              },
        { "Anas_platyrhynchos","Duck"                },
        { "Cervus_elaphus",   "Deer (Venison)"       },
        { "Meleagris_gallopavo","Turkey"             },
        { "Oryctolagus_cuniculus","Rabbit"            },
        { "Camelus_dromedarius","Camel"              },
        { "Canis_lupus",      "Dog"                  },
        { "Equus_asinus",     "Donkey"               },
        { NULL, NULL }
    };
    for (int i = 0; names[i].latin; i++) {
        if (strcmp(species_id, names[i].latin) == 0)
            return names[i].common;
    }
    return species_id;  /* fallback to Latin name */
}

/* Friendly verdict text */
static const char *friendly_verdict(verdict_t v) {
    switch (v) {
        case PASS: return "PASS - No prohibited content detected";
        case FAIL: return "FAIL - Prohibited content detected";
        default:   return "INCONCLUSIVE - Unable to determine";
    }
}

/* Friendly status text */
static const char *friendly_status(halal_status_t s) {
    switch (s) {
        case HALAL:    return "Halal";
        case HARAM:    return "Haram";
        case MASHBOOH: return "Doubtful";
        default:       return "Unknown";
    }
}

/* Confidence level from cross-marker agreement */
static const char *confidence_label(double agreement) {
    if (agreement >= 0.95) return "Very High";
    if (agreement >= 0.85) return "High";
    if (agreement >= 0.70) return "Moderate";
    return "Low";
}

/* ================================================================== */
/* File dialog — multi-select                                          */
/* ================================================================== */
static void open_file_dialog(gui_state_t *st) {
    const char *filters[] = { "*.fq", "*.fastq", "*.fq.gz", "*.fastq.gz",
                               "*.fa", "*.fasta", "*.fa.gz", "*.fasta.gz" };
    const char *result = tinyfd_openFileDialog(
        "Select DNA sample file(s)",           /* title        */
        "",                                    /* default path */
        8, filters,                            /* filter       */
        "DNA sample files (*.fq *.fa *.gz)",   /* description  */
        1                                      /* multi-select */
    );
    if (!result) return;

    /* Parse pipe-separated paths (tinyfiledialogs multi-select format) */
    analysis_context_t *a = &st->analysis;
    a->n_fastq_files = 0;
    const char *p = result;
    while (*p && a->n_fastq_files < 32) {
        const char *sep = strchr(p, '|');
        int len = sep ? (int)(sep - p) : (int)strlen(p);
        if (len > 0 && len < 1024) {
            memcpy(a->fastq_paths[a->n_fastq_files], p, (size_t)len);
            a->fastq_paths[a->n_fastq_files][len] = '\0';
            a->n_fastq_files++;
        }
        if (!sep) break;
        p = sep + 1;
    }
    detect_samples(a);
}

/* ================================================================== */
/* Locate default index file                                           */
/* ================================================================== */
static void find_default_index(char *path, size_t sz) {
#ifdef __APPLE__
    /* macOS: look inside .app bundle first */
    {
        char buf[1024];
        const char *base = SDL_GetBasePath();
        if (base) {
            snprintf(buf, sizeof(buf), "%s../Resources/default.idx", base);
            FILE *f = fopen(buf, "rb");
            if (f) { fclose(f); snprintf(path, sz, "%s", buf); return; }
        }
    }
#endif
    /* Same directory as executable */
    {
        const char *base = SDL_GetBasePath();
        if (base) {
            char buf[1024];
            snprintf(buf, sizeof(buf), "%sdefault.idx", base);
            FILE *f = fopen(buf, "rb");
            if (f) { fclose(f); snprintf(path, sz, "%s", buf); return; }
            /* Also check for speciesid.idx next to exe */
            snprintf(buf, sizeof(buf), "%sspeciesid.idx", base);
            f = fopen(buf, "rb");
            if (f) { fclose(f); snprintf(path, sz, "%s", buf); return; }
        }
    }
    /* Current working directory */
    {
        FILE *f = fopen("speciesid.idx", "rb");
        if (f) { fclose(f); snprintf(path, sz, "speciesid.idx"); return; }
    }
    path[0] = '\0';
}

/* ================================================================== */
/* Load database info from index for database viewer                   */
/* ================================================================== */
static halal_refdb_t *load_db_info(const char *index_path) {
    if (!index_path[0]) return NULL;
    halal_index_t *idx = index_load(index_path);
    if (!idx) return NULL;
    /* Steal the db pointer, then free index parts manually
       (index_destroy dereferences idx->db, so we can't NULL it) */
    halal_refdb_t *db = idx->db;
    int S = db->n_species;
    int M = db->n_markers;
    for (int s = 0; s < S; s++) fmh_destroy(idx->coarse[s]);
    free(idx->coarse);
    for (int m = 0; m < M; m++) {
        for (int s = 0; s < S; s++) kmer_set_destroy(idx->fine[m][s]);
        free(idx->fine[m]);
    }
    free(idx->fine);
    for (int m = 0; m < M; m++) kmer_set_destroy(idx->primer_index[m]);
    free(idx->primer_index);
    free(idx);  /* don't call refdb_destroy — we're keeping db */
    return db;
}

/* ================================================================== */
/* Format file size                                                    */
/* ================================================================== */
static void format_bytes(long bytes, char *out, size_t sz) {
    if (bytes < 1024)
        snprintf(out, sz, "%ld B", bytes);
    else if (bytes < 1024 * 1024)
        snprintf(out, sz, "%.1f KB", bytes / 1024.0);
    else if (bytes < 1024L * 1024 * 1024)
        snprintf(out, sz, "%.1f MB", bytes / (1024.0 * 1024.0));
    else
        snprintf(out, sz, "%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
}

/* ================================================================== */
/* Draw horizontal bar chart with CI whiskers                          */
/* ================================================================== */
static void draw_horizontal_bars(struct nk_context *ctx,
                                  const halal_report_t *r,
                                  float chart_width,
                                  float min_display_pct)
{
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    float name_col = 140.0f;
    float pct_col = 64.0f;
    float bar_area = chart_width - name_col - pct_col - 20.0f;
    if (bar_area < 40.0f) bar_area = 40.0f;

    /* Count hidden species (below threshold but non-zero) */
    int n_hidden = 0;
    for (int i = 0; i < r->n_species; i++) {
        const species_report_t *sp = &r->species[i];
        if (sp->weight_pct < min_display_pct && sp->read_pct < min_display_pct
            && (sp->weight_pct > 0.0001 || sp->read_pct > 0.0001))
            n_hidden++;
    }

    /* Find max value for scaling (among visible species) */
    double max_val = 0.0;
    for (int i = 0; i < r->n_species; i++) {
        const species_report_t *sp = &r->species[i];
        if (sp->weight_pct < min_display_pct && sp->read_pct < min_display_pct)
            continue;
        double hi = sp->ci_hi;
        if (hi > max_val) max_val = hi;
        if (sp->weight_pct > max_val) max_val = sp->weight_pct;
    }
    if (max_val < 1.0) max_val = 1.0;
    double scale_max = max_val * 1.1;
    if (scale_max > 100.0) scale_max = 100.0;

    int row_idx = 0;
    for (int i = 0; i < r->n_species; i++) {
        const species_report_t *sp = &r->species[i];
        if (sp->weight_pct < min_display_pct && sp->read_pct < min_display_pct)
            continue;

        nk_layout_row_dynamic(ctx, 28, 1);
        struct nk_rect row_bounds;
        nk_widget(&row_bounds, ctx);

        float x0 = row_bounds.x;
        float y_mid = row_bounds.y + row_bounds.h * 0.5f;

        /* Alternating row background */
        if (row_idx % 2 == 1) {
            nk_fill_rect(canvas, row_bounds, 0,
                         nk_rgba(255, 255, 255, 8));
        }
        row_idx++;

        /* Species name */
        struct nk_rect name_rect = nk_rect(x0, row_bounds.y,
                                            name_col, row_bounds.h);
        nk_draw_text(canvas, name_rect,
                     friendly_species_name(sp->species_id),
                     (int)strlen(friendly_species_name(sp->species_id)),
                     ctx->style.font, nk_rgba(0,0,0,0),
                     nk_rgb(210, 210, 210));

        /* Bar track (dim background) */
        float bar_x = x0 + name_col;
        float bar_h = row_bounds.h - 8.0f;
        float bar_y = row_bounds.y + 4.0f;
        nk_fill_rect(canvas, nk_rect(bar_x, bar_y, bar_area, bar_h),
                      4, nk_rgba(255, 255, 255, 15));

        /* Colored bar */
        float bar_w = (float)(sp->weight_pct / scale_max) * bar_area;
        if (bar_w < 3.0f && sp->weight_pct > 0.0001) bar_w = 3.0f;
        struct nk_color bar_color = status_color(sp->halal_status);
        if (bar_w > 0.5f)
            nk_fill_rect(canvas, nk_rect(bar_x, bar_y, bar_w, bar_h),
                          4, bar_color);

        /* CI whiskers (bar color at 40% opacity) */
        if (sp->ci_lo >= 0 && sp->ci_hi > 0) {
            float ci_lo_x = bar_x + (float)(sp->ci_lo / scale_max) * bar_area;
            float ci_hi_x = bar_x + (float)(sp->ci_hi / scale_max) * bar_area;
            float whisker_h = bar_h * 0.6f;
            struct nk_color wc = nk_rgba(bar_color.r, bar_color.g,
                                          bar_color.b, 100);
            nk_stroke_line(canvas, ci_lo_x, y_mid, ci_hi_x, y_mid, 2.0f, wc);
            nk_stroke_line(canvas, ci_lo_x, y_mid - whisker_h/2,
                           ci_lo_x, y_mid + whisker_h/2, 1.5f, wc);
            nk_stroke_line(canvas, ci_hi_x, y_mid - whisker_h/2,
                           ci_hi_x, y_mid + whisker_h/2, 1.5f, wc);
        }

        /* Percentage text (right-aligned) */
        char pct_buf[32];
        if (sp->weight_pct > 0.0001 && sp->weight_pct < 0.1)
            snprintf(pct_buf, sizeof(pct_buf), "< 0.1%%");
        else
            snprintf(pct_buf, sizeof(pct_buf), "%5.1f%%", sp->weight_pct);
        float pct_x = x0 + name_col + bar_area + 4.0f;
        struct nk_rect pct_rect = nk_rect(pct_x, row_bounds.y,
                                           pct_col, row_bounds.h);
        nk_draw_text(canvas, pct_rect, pct_buf, (int)strlen(pct_buf),
                     ctx->style.font, nk_rgba(0,0,0,0),
                     nk_rgb(210, 210, 210));
    }

    /* "N more below threshold" */
    if (n_hidden > 0) {
        nk_layout_row_dynamic(ctx, 22, 1);
        char hidden_buf[64];
        snprintf(hidden_buf, sizeof(hidden_buf),
                 "%d more below %.1f%% threshold", n_hidden, min_display_pct);
        nk_label_colored(ctx, hidden_buf, NK_TEXT_LEFT,
                         nk_rgb(120, 120, 120));
    }
}

/* ================================================================== */
/* Draw stacked species bar (bottom of left panel)                     */
/* ================================================================== */
static void draw_species_bar(struct nk_context *ctx, const halal_report_t *r) {
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    struct nk_rect bounds;
    nk_layout_row_dynamic(ctx, ROW_BAR, 1);
    nk_widget(&bounds, ctx);

    /* Rounded background track */
    nk_fill_rect(canvas, bounds, 4, nk_rgba(255, 255, 255, 15));

    /* Count visible segments */
    int n_vis = 0;
    for (int i = 0; i < r->n_species; i++) {
        float w = (float)(r->species[i].weight_pct / 100.0) * bounds.w;
        if (w >= 1.0f) n_vis++;
    }

    /* Draw segments with 1px dark gap between them */
    float x = bounds.x;
    int seg = 0;
    for (int i = 0; i < r->n_species; i++) {
        float w = (float)(r->species[i].weight_pct / 100.0) * bounds.w;
        if (w < 1.0f) continue;

        /* 1px gap between segments (not before first or after last) */
        float gap = (seg > 0 && seg < n_vis) ? 1.0f : 0.0f;
        float draw_x = x + gap;
        float draw_w = w - gap;
        if (draw_w < 1.0f) draw_w = 1.0f;

        struct nk_color c = status_color(r->species[i].halal_status);

        /* Round left end of first segment, right end of last */
        float rounding = 0;
        if (seg == 0 && seg == n_vis - 1)
            rounding = 4;  /* only segment: round both ends */
        else if (seg == 0)
            rounding = 4;  /* first segment */
        else if (seg == n_vis - 1)
            rounding = 4;  /* last segment */

        nk_fill_rect(canvas, nk_rect(draw_x, bounds.y, draw_w, bounds.h),
                      rounding, c);
        x += w;
        seg++;
    }
}

/* ================================================================== */
/* Draw database viewer panel                                          */
/* ================================================================== */
static void draw_database_panel(struct nk_context *ctx,
                                 const halal_refdb_t *db)
{
    if (!db) {
        nk_layout_row_dynamic(ctx, 50, 1);
        nk_label_wrap(ctx, "No database loaded. Ensure the index file "
                      "is available.");
        return;
    }

    /* Summary line */
    nk_layout_row_dynamic(ctx, ROW_LABEL, 1);
    {
        char summary[128];
        snprintf(summary, sizeof(summary),
                 "%d species, %d markers, %d references",
                 db->n_species, db->n_markers, db->n_marker_refs);
        nk_label(ctx, summary, NK_TEXT_LEFT);
    }

    nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
    nk_spacing(ctx, 1);

    /* --- Species table --- */
    nk_layout_row_dynamic(ctx, ROW_LABEL, 1);
    nk_label(ctx, "Species", NK_TEXT_LEFT);

    /* Header */
    nk_layout_row_dynamic(ctx, ROW_TABLE, 4);
    nk_label(ctx, "Species ID",   NK_TEXT_LEFT);
    nk_label(ctx, "Common Name",  NK_TEXT_LEFT);
    nk_label(ctx, "Status",       NK_TEXT_CENTERED);
    nk_label(ctx, "Mito CN",      NK_TEXT_RIGHT);

    for (int s = 0; s < db->n_species; s++) {
        nk_layout_row_dynamic(ctx, ROW_TABLE, 4);
        nk_label(ctx, db->species[s].species_id, NK_TEXT_LEFT);
        nk_label(ctx, db->species[s].common_name, NK_TEXT_LEFT);
        nk_label_colored(ctx, friendly_status(db->species[s].status),
                         NK_TEXT_CENTERED,
                         status_color(db->species[s].status));
        char cn_buf[32];
        snprintf(cn_buf, sizeof(cn_buf), "%.0f",
                 db->species[s].mito_copy_number);
        nk_label(ctx, cn_buf, NK_TEXT_RIGHT);
    }

    nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
    nk_spacing(ctx, 1);

    /* --- Markers table --- */
    nk_layout_row_dynamic(ctx, ROW_LABEL, 1);
    nk_label(ctx, "Markers & Primers", NK_TEXT_LEFT);

    nk_layout_row_dynamic(ctx, ROW_TABLE, 3);
    nk_label(ctx, "Marker",       NK_TEXT_LEFT);
    nk_label(ctx, "Forward",      NK_TEXT_LEFT);
    nk_label(ctx, "Reverse",      NK_TEXT_LEFT);

    for (int m = 0; m < db->n_markers; m++) {
        nk_layout_row_dynamic(ctx, ROW_TABLE, 3);
        nk_label(ctx, db->marker_ids[m], NK_TEXT_LEFT);
        nk_label(ctx, db->primer_f[m][0] ? db->primer_f[m] : "-",
                 NK_TEXT_LEFT);
        nk_label(ctx, db->primer_r[m][0] ? db->primer_r[m] : "-",
                 NK_TEXT_LEFT);
    }

    nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
    nk_spacing(ctx, 1);

    /* --- Coverage matrix --- */
    nk_layout_row_dynamic(ctx, ROW_LABEL, 1);
    nk_label(ctx, "Reference Coverage (amplicon bp)", NK_TEXT_LEFT);

    /* Header row: blank + marker names */
    int n_cols = db->n_markers + 1;
    nk_layout_row_dynamic(ctx, ROW_TABLE, n_cols);
    nk_label(ctx, "Species", NK_TEXT_LEFT);
    for (int m = 0; m < db->n_markers; m++)
        nk_label(ctx, db->marker_ids[m], NK_TEXT_CENTERED);

    /* Data rows */
    for (int s = 0; s < db->n_species; s++) {
        nk_layout_row_dynamic(ctx, ROW_TABLE, n_cols);
        nk_label(ctx, friendly_species_name(db->species[s].species_id),
                 NK_TEXT_LEFT);
        for (int m = 0; m < db->n_markers; m++) {
            marker_ref_t *mr = refdb_get_marker_ref(db, s, m);
            char cell[16];
            if (mr && mr->seq_len > 0)
                snprintf(cell, sizeof(cell), "%d", mr->seq_len);
            else
                snprintf(cell, sizeof(cell), "-");
            nk_label(ctx, cell, NK_TEXT_CENTERED);
        }
    }
}

/* ================================================================== */
/* First-launch wizard                                                 */
/* ================================================================== */

/* sid_config_dir() is now provided by src/update.c via update.h */

/* Check if setup_done marker exists */
static int wizard_setup_done_exists(void) {
    char dir[1024];
    if (!sid_config_dir(dir, sizeof(dir))) return 0;
    char path[1024];
    snprintf(path, sizeof(path), "%s%csetup_done", dir, '/');
    FILE *f = fopen(path, "r");
    if (f) { fclose(f); return 1; }
    return 0;
}

/* Create setup_done marker */
static void wizard_write_setup_done(void) {
    char dir[1024];
    if (!sid_config_dir(dir, sizeof(dir))) return;
    SID_MKDIR(dir);
    char path[1024];
    snprintf(path, sizeof(path), "%s%csetup_done", dir, '/');
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "1\n"); fclose(f); }
}

/* Background thread for index build from wizard */
static int wizard_build_worker(void *data) {
    gui_state_t *st = (gui_state_t *)data;

    /* Build refdb then index via system() calls to speciesid CLI */
    const char *base = SDL_GetBasePath();
    char cmd[2048];
#ifdef _WIN32
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = "C:\\Temp";
    if (base && base[0]) {
        snprintf(cmd, sizeof(cmd),
                 "\"%sspeciesid.exe\" build-db -o \"%s\\_sid_wizard.db\" && "
                 "\"%sspeciesid.exe\" index -d \"%s\\_sid_wizard.db\" -o speciesid.idx && "
                 "del \"%s\\_sid_wizard.db\"",
                 base, tmp, base, tmp, tmp);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "speciesid.exe build-db -o \"%s\\_sid_wizard.db\" && "
                 "speciesid.exe index -d \"%s\\_sid_wizard.db\" -o speciesid.idx && "
                 "del \"%s\\_sid_wizard.db\"",
                 tmp, tmp, tmp);
    }
#else
    if (base && base[0]) {
        snprintf(cmd, sizeof(cmd),
                 "%sspeciesid build-db -o /tmp/_sid_wizard.db && "
                 "%sspeciesid index -d /tmp/_sid_wizard.db -o speciesid.idx && "
                 "rm -f /tmp/_sid_wizard.db",
                 base, base);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "./speciesid build-db -o /tmp/_sid_wizard.db && "
                 "./speciesid index -d /tmp/_sid_wizard.db -o speciesid.idx && "
                 "rm -f /tmp/_sid_wizard.db");
    }
#endif
    int ret = system(cmd);
    if (ret == 0) {
        find_default_index(st->index_path, sizeof(st->index_path));
        if (st->index_path[0]) {
            st->index_found = 1;
            if (st->db_info) refdb_destroy(st->db_info);
            st->db_info = load_db_info(st->index_path);
        }
    }
    st->wizard_building = 0;
    return ret;
}

/* ================================================================== */
/* Update background workers                                           */
/* ================================================================== */

/* Background thread: check for updates (fetch manifest, compare ver) */
static int update_check_worker(void *data) {
    gui_state_t *st = (gui_state_t *)data;
    sid_update_ctx_t *uc = &st->update_ctx;

    /* Ensure index_path is set in update context */
    if (!uc->index_path[0] && st->index_path[0])
        snprintf(uc->index_path, sizeof(uc->index_path), "%s", st->index_path);

    sid_update_run(uc);

    if (uc->status == SID_UPDATE_AVAILABLE)
        st->update_available = 1;

    st->update_checking = 0;
    return 0;
}

/* Background thread: download, verify, install the update */
static int update_download_worker(void *data) {
    gui_state_t *st = (gui_state_t *)data;
    sid_update_ctx_t *uc = &st->update_ctx;

    sid_update_run_download(uc);

    if (uc->status == SID_UPDATE_DONE) {
        /* Reload database info from the newly installed index */
        if (st->db_info) refdb_destroy(st->db_info);
        st->db_info = load_db_info(st->index_path);
        st->update_available = 0;
    }

    st->update_in_progress = 0;
    return 0;
}

/* Draw custom step progress bar with circles and connecting lines */
static void draw_step_progress(struct nk_context *ctx, int current_step)
{
    static const char *step_labels[] = {
        "Welcome", "Index", "Formats", "Ready"
    };
    nk_layout_row_dynamic(ctx, 50, 1);
    struct nk_rect bounds;
    nk_widget(&bounds, ctx);
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

    const struct nk_user_font *font = ctx->style.font;
    float total_w = bounds.w - 100.0f;
    float x_start = bounds.x + 50.0f;
    float y_center = bounds.y + 14.0f;
    float spacing = total_w / 3.0f;
    float radius = 12.0f;

    /* Draw connecting lines first (behind circles) */
    for (int i = 0; i < 3; i++) {
        float x1 = x_start + (float)i * spacing + radius + 2;
        float x2 = x_start + (float)(i + 1) * spacing - radius - 2;
        struct nk_color line_col = (i < current_step)
            ? nk_rgb(34, 139, 34) : nk_rgb(70, 70, 75);
        nk_stroke_line(canvas, x1, y_center, x2, y_center, 2.0f, line_col);
    }

    /* Draw circles and labels */
    for (int i = 0; i < 4; i++) {
        float cx = x_start + (float)i * spacing;
        struct nk_color fill, border_c, text_c;

        if (i < current_step) {
            /* Completed: solid green */
            fill = nk_rgb(34, 139, 34);
            border_c = nk_rgb(40, 160, 40);
            text_c = nk_rgb(255, 255, 255);
        } else if (i == current_step) {
            /* Current: green outline, dark fill */
            fill = nk_rgb(25, 60, 25);
            border_c = nk_rgb(34, 139, 34);
            text_c = nk_rgb(34, 139, 34);
        } else {
            /* Future: dim */
            fill = nk_rgb(50, 50, 55);
            border_c = nk_rgb(70, 70, 75);
            text_c = nk_rgb(100, 100, 100);
        }

        /* Filled circle */
        nk_fill_circle(canvas,
            nk_rect(cx - radius, y_center - radius,
                    radius * 2, radius * 2),
            fill);
        nk_stroke_circle(canvas,
            nk_rect(cx - radius, y_center - radius,
                    radius * 2, radius * 2),
            2.0f, border_c);

        /* Step number inside circle — manually centered */
        char num[4];
        if (i < current_step)
            snprintf(num, sizeof(num), "ok");
        else
            snprintf(num, sizeof(num), "%d", i + 1);
        int num_len = (int)strlen(num);
        float text_w = font->width(font->userdata, font->height,
                                    num, num_len);
        float text_h = font->height;
        float tx = cx - text_w * 0.5f;
        float ty = y_center - text_h * 0.5f;
        nk_draw_text(canvas, nk_rect(tx, ty, text_w + 4, text_h + 2),
                     num, num_len,
                     font, nk_rgba(0,0,0,0), text_c);

        /* Label below — centered under circle */
        int lbl_len = (int)strlen(step_labels[i]);
        float lbl_w = font->width(font->userdata, font->height,
                                   step_labels[i], lbl_len);
        float lbl_x = cx - lbl_w * 0.5f;
        float lbl_y = y_center + radius + 4;
        struct nk_color lbl_c = (i <= current_step)
            ? nk_rgb(200, 200, 200) : nk_rgb(100, 100, 100);
        nk_draw_text(canvas,
                     nk_rect(lbl_x, lbl_y, lbl_w + 4, 16),
                     step_labels[i], lbl_len,
                     font, nk_rgba(0,0,0,0), lbl_c);
    }
}

/* Draw an animated loading bar */
static void draw_loading_bar(struct nk_context *ctx, Uint32 start_ticks)
{
    nk_layout_row_dynamic(ctx, 16, 1);
    struct nk_rect bounds;
    nk_widget(&bounds, ctx);
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

    /* Track background */
    nk_fill_rect(canvas, bounds, 4, nk_rgb(50, 50, 55));

    /* Animated sweeping bar */
    Uint32 elapsed = SDL_GetTicks() - start_ticks;
    float cycle = (float)(elapsed % 2000) / 2000.0f;  /* 0..1 over 2 sec */
    /* Ease in/out with sine */
    float pos = (1.0f - cosf(cycle * 3.14159f * 2.0f)) * 0.5f;
    float bar_w = bounds.w * 0.35f;
    float bar_x = bounds.x + pos * (bounds.w - bar_w);

    /* Clamp to track bounds */
    if (bar_x < bounds.x) bar_x = bounds.x;
    if (bar_x + bar_w > bounds.x + bounds.w)
        bar_w = bounds.x + bounds.w - bar_x;

    nk_fill_rect(canvas,
                  nk_rect(bar_x, bounds.y, bar_w, bounds.h),
                  4, nk_rgb(34, 139, 34));
}

/* Draw the wizard content inside a group within the main window.
   Caller must already have nk_begin'd the main window. */
static void draw_wizard_content(struct nk_context *ctx, gui_state_t *st,
                                 float pw)
{
    /* Ensure animation timer is set */
    if (st->wizard_start_ticks == 0)
        st->wizard_start_ticks = SDL_GetTicks();

    /* Top padding */
    nk_layout_row_dynamic(ctx, 10, 1);
    nk_spacing(ctx, 1);

    /* Step progress bar */
    draw_step_progress(ctx, st->wizard_step);

    /* Space below progress dots+labels */
    nk_layout_row_dynamic(ctx, 20, 1);
    nk_spacing(ctx, 1);

    /* Separator line */
    nk_layout_row_dynamic(ctx, 2, 1);
    {
        struct nk_rect sep;
        nk_widget(&sep, ctx);
        struct nk_command_buffer *c = nk_window_get_canvas(ctx);
        nk_fill_rect(c, nk_rect(sep.x + 20, sep.y, sep.w - 40, 1),
                      0, nk_rgb(60, 60, 65));
    }

    nk_layout_row_dynamic(ctx, 12, 1);
    nk_spacing(ctx, 1);

    /* Content area per step */
    switch (st->wizard_step) {
    case 0: /* Welcome */
        nk_layout_row_dynamic(ctx, 40, 1);
        nk_label(ctx, "Welcome to SpeciesID", NK_TEXT_CENTERED);

        nk_layout_row_dynamic(ctx, 8, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 50, 1);
        nk_label_wrap(ctx,
            "Halal food authentication via DNA metabarcoding. "
            "This wizard will check that everything is set up "
            "for accurate analysis.");

        nk_layout_row_dynamic(ctx, 12, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 50, 1);
        nk_label_wrap(ctx,
            "SpeciesID identifies animal species in food samples "
            "using mitochondrial DNA markers and statistical "
            "estimation.");

        nk_layout_row_dynamic(ctx, 12, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
        nk_label_colored(ctx, "Click Next to get started.",
                         NK_TEXT_CENTERED, nk_rgb(140, 140, 140));
        break;

    case 1: /* Reference Index */
        nk_layout_row_dynamic(ctx, 34, 1);
        nk_label(ctx, "Reference Index", NK_TEXT_CENTERED);

        nk_layout_row_dynamic(ctx, 8, 1);
        nk_spacing(ctx, 1);

        if (st->wizard_building) {
            nk_layout_row_dynamic(ctx, ROW_LABEL, 1);
            nk_label_colored(ctx, "Building reference database...",
                             NK_TEXT_CENTERED, nk_rgb(218, 165, 32));

            nk_layout_row_dynamic(ctx, 8, 1);
            nk_spacing(ctx, 1);

            draw_loading_bar(ctx, st->wizard_start_ticks);

            nk_layout_row_dynamic(ctx, 12, 1);
            nk_spacing(ctx, 1);

            nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
            nk_label_colored(ctx, "This may take a moment...",
                             NK_TEXT_CENTERED, nk_rgb(120, 120, 120));

        } else if (st->index_found) {
            /* Green status box */
            nk_layout_row_dynamic(ctx, 28, 1);
            {
                struct nk_rect bx;
                nk_widget(&bx, ctx);
                struct nk_command_buffer *c = nk_window_get_canvas(ctx);
                nk_fill_rect(c, bx, 6, nk_rgba(34, 139, 34, 30));
                nk_stroke_rect(c, bx, 6, 1.0f, nk_rgba(34, 139, 34, 80));
                nk_draw_text(c, nk_rect(bx.x + 10, bx.y, bx.w - 20, bx.h),
                    "Index found", 11,
                    ctx->style.font, nk_rgba(0,0,0,0),
                    nk_rgb(34, 200, 34));
            }

            nk_layout_row_dynamic(ctx, 8, 1);
            nk_spacing(ctx, 1);

            nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
            {
                char path_label[256];
                snprintf(path_label, sizeof(path_label),
                         "Path: %s", st->index_path);
                nk_label_colored(ctx, path_label, NK_TEXT_LEFT,
                                 nk_rgb(160, 160, 160));
            }

            if (st->db_info) {
                nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
                char info[128];
                snprintf(info, sizeof(info),
                         "Contains %d species across %d markers",
                         st->db_info->n_species,
                         st->db_info->n_markers);
                nk_label_colored(ctx, info, NK_TEXT_LEFT,
                                 nk_rgb(160, 160, 160));
            }

            /* --- Check for Updates button (wizard step 1) --- */
            nk_layout_row_dynamic(ctx, 8, 1);
            nk_spacing(ctx, 1);

            if (st->update_checking) {
                nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
                nk_label_colored(ctx, "Checking for updates...",
                                 NK_TEXT_LEFT, nk_rgb(100, 180, 255));
                draw_loading_bar(ctx, st->wizard_start_ticks);
            } else if (st->update_available) {
                nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
                char upd_info[128];
                snprintf(upd_info, sizeof(upd_info),
                         "Update available: v%u -> v%u",
                         st->update_ctx.local_version,
                         st->update_ctx.manifest.version);
                nk_label_colored(ctx, upd_info, NK_TEXT_LEFT,
                                 nk_rgb(100, 180, 255));
                if (st->update_in_progress) {
                    nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
                    int us = st->update_ctx.status;
                    const char *msg = (us == SID_UPDATE_DOWNLOADING) ? "Downloading..." :
                                      (us == SID_UPDATE_VERIFYING)   ? "Verifying..." :
                                      (us == SID_UPDATE_INSTALLING)  ? "Installing..." :
                                      (us == SID_UPDATE_DONE)        ? "Update complete!" :
                                      (us == SID_UPDATE_ERROR)       ? st->update_ctx.error_msg :
                                      "Updating...";
                    struct nk_color mc = (us == SID_UPDATE_DONE)  ? nk_rgb(34, 200, 34) :
                                         (us == SID_UPDATE_ERROR) ? nk_rgb(220, 100, 100) :
                                         nk_rgb(218, 165, 32);
                    nk_label_colored(ctx, msg, NK_TEXT_LEFT, mc);
                    draw_loading_bar(ctx, st->wizard_start_ticks);
                } else {
                    nk_layout_row_dynamic(ctx, ROW_BTN, 1);
                    struct nk_style_button upd = ctx->style.button;
                    upd.normal = nk_style_item_color(nk_rgb(30, 100, 180));
                    upd.hover  = nk_style_item_color(nk_rgb(40, 120, 210));
                    upd.active = nk_style_item_color(nk_rgb(20, 80, 150));
                    upd.text_normal = nk_rgb(255, 255, 255);
                    upd.text_hover  = nk_rgb(255, 255, 255);
                    upd.text_active = nk_rgb(255, 255, 255);
                    upd.rounding = 6;
                    if (nk_button_label_styled(ctx, &upd, "Update Now")) {
                        st->update_in_progress = 1;
                        st->wizard_start_ticks = SDL_GetTicks();
                        SDL_DetachThread(
                            SDL_CreateThread(update_download_worker, "update_dl", st));
                    }
                }
            } else if (st->update_ctx.status == SID_UPDATE_ERROR) {
                nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
                nk_label_colored(ctx, "Update check failed (offline?)",
                                 NK_TEXT_LEFT, nk_rgb(120, 120, 120));
            } else {
                nk_layout_row_dynamic(ctx, ROW_BTN, 1);
                if (nk_button_label(ctx, "Check for Updates")) {
                    st->update_checking = 1;
                    st->wizard_start_ticks = SDL_GetTicks();
                    SDL_DetachThread(
                        SDL_CreateThread(update_check_worker, "update_chk", st));
                }
            }
        } else {
            /* Red status box */
            nk_layout_row_dynamic(ctx, 28, 1);
            {
                struct nk_rect bx;
                nk_widget(&bx, ctx);
                struct nk_command_buffer *c = nk_window_get_canvas(ctx);
                nk_fill_rect(c, bx, 6, nk_rgba(220, 20, 60, 25));
                nk_stroke_rect(c, bx, 6, 1.0f, nk_rgba(220, 20, 60, 80));
                const char *msg = "No reference index found";
                nk_draw_text(c, nk_rect(bx.x + 10, bx.y, bx.w - 20, bx.h),
                    msg, (int)strlen(msg),
                    ctx->style.font, nk_rgba(0,0,0,0),
                    nk_rgb(220, 100, 100));
            }

            nk_layout_row_dynamic(ctx, 8, 1);
            nk_spacing(ctx, 1);

            nk_layout_row_dynamic(ctx, 44, 1);
            nk_label_wrap(ctx,
                "A reference index is required for species identification. "
                "Click below to build one from built-in references.");

            nk_layout_row_dynamic(ctx, 8, 1);
            nk_spacing(ctx, 1);

            nk_layout_row_dynamic(ctx, ROW_BTN, 1);
            {
                struct nk_style_button build = ctx->style.button;
                build.normal = nk_style_item_color(nk_rgb(34, 100, 34));
                build.hover  = nk_style_item_color(nk_rgb(34, 139, 34));
                build.active = nk_style_item_color(nk_rgb(20, 80, 20));
                build.text_normal = nk_rgb(255, 255, 255);
                build.text_hover  = nk_rgb(255, 255, 255);
                build.text_active = nk_rgb(255, 255, 255);
                build.rounding = 6;
                if (nk_button_label_styled(ctx, &build, "Build Index")) {
                    st->wizard_building = 1;
                    st->wizard_start_ticks = SDL_GetTicks();
                    SDL_CreateThread(wizard_build_worker, "wizard_build", st);
                }
            }
        }
        break;

    case 2: /* Supported Formats */
        nk_layout_row_dynamic(ctx, 34, 1);
        nk_label(ctx, "Supported Formats", NK_TEXT_CENTERED);

        nk_layout_row_dynamic(ctx, 8, 1);
        nk_spacing(ctx, 1);

        /* Format chips in a styled box */
        nk_layout_row_dynamic(ctx, 56, 1);
        {
            struct nk_rect bx;
            nk_widget(&bx, ctx);
            struct nk_command_buffer *c = nk_window_get_canvas(ctx);
            nk_fill_rect(c, bx, 6, nk_rgba(255, 255, 255, 8));

            const char *line1 = "  FASTQ:  .fq   .fastq   .fq.gz   .fastq.gz";
            const char *line2 = "  FASTA:  .fa   .fasta   .fa.gz   .fasta.gz";
            nk_draw_text(c,
                nk_rect(bx.x + 8, bx.y + 4, bx.w - 16, 22),
                line1, (int)strlen(line1),
                ctx->style.font, nk_rgba(0,0,0,0),
                nk_rgb(180, 220, 180));
            nk_draw_text(c,
                nk_rect(bx.x + 8, bx.y + 28, bx.w - 16, 22),
                line2, (int)strlen(line2),
                ctx->style.font, nk_rgba(0,0,0,0),
                nk_rgb(180, 220, 180));
        }

        nk_layout_row_dynamic(ctx, 12, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 40, 1);
        nk_label_wrap(ctx,
            "No preprocessing required - SpeciesID works directly "
            "with raw sequencing data.");

        nk_layout_row_dynamic(ctx, 40, 1);
        nk_label_wrap(ctx,
            "For best results, use amplicon-targeted sequencing "
            "(PCR + Illumina).");

        nk_layout_row_dynamic(ctx, 40, 1);
        nk_label_wrap(ctx,
            "R1/R2 paired-end files are automatically detected "
            "and merged into a single sample.");
        break;

    case 3: /* Ready */
        nk_layout_row_dynamic(ctx, 34, 1);
        nk_label(ctx, "All Set!", NK_TEXT_CENTERED);

        nk_layout_row_dynamic(ctx, 12, 1);
        nk_spacing(ctx, 1);

        /* Green success box */
        nk_layout_row_dynamic(ctx, 60, 1);
        {
            struct nk_rect bx;
            nk_widget(&bx, ctx);
            struct nk_command_buffer *c = nk_window_get_canvas(ctx);
            nk_fill_rect(c, bx, 8, nk_rgba(34, 139, 34, 25));
            nk_stroke_rect(c, bx, 8, 1.0f, nk_rgba(34, 139, 34, 60));
            const char *ok_msg = "Setup complete - ready to analyze samples!";
            nk_draw_text(c,
                nk_rect(bx.x + 12, bx.y, bx.w - 24, bx.h),
                ok_msg, (int)strlen(ok_msg),
                ctx->style.font, nk_rgba(0,0,0,0),
                nk_rgb(34, 200, 34));
        }

        nk_layout_row_dynamic(ctx, 12, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 50, 1);
        nk_label_wrap(ctx,
            "Choose DNA sample files and click Run Analysis to "
            "check halal status of food products.");

        nk_layout_row_dynamic(ctx, 20, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
        nk_label_colored(ctx, "Tip: re-run wizard by deleting ~/.speciesid/setup_done",
                         NK_TEXT_CENTERED, nk_rgb(100, 100, 100));
        break;
    }

    /* Flexible spacer — grow to push buttons to bottom.
       Calculate remaining space: ph minus what's used so far.
       We allocate a generous minimum to keep buttons at bottom. */
    nk_layout_row_dynamic(ctx, 10, 1);
    nk_spacing(ctx, 1);

    /* Separator above buttons */
    nk_layout_row_dynamic(ctx, 2, 1);
    {
        struct nk_rect sep;
        nk_widget(&sep, ctx);
        struct nk_command_buffer *c = nk_window_get_canvas(ctx);
        nk_fill_rect(c, nk_rect(sep.x + 20, sep.y, sep.w - 40, 1),
                      0, nk_rgb(60, 60, 65));
    }

    nk_layout_row_dynamic(ctx, 8, 1);
    nk_spacing(ctx, 1);

    /* Navigation buttons — 3 columns: [Skip] [      ] [Back  Next] */
    {
        float btn_widths[] = { 80, pw - 280, 80, 80 };
        nk_layout_row(ctx, NK_STATIC, 36, 4, btn_widths);

        /* Skip button (step 0 only) */
        if (st->wizard_step == 0) {
            struct nk_style_button skip = ctx->style.button;
            skip.normal = nk_style_item_color(nk_rgba(0,0,0,0));
            skip.hover  = nk_style_item_color(nk_rgba(255,255,255,10));
            skip.active = nk_style_item_color(nk_rgba(255,255,255,5));
            skip.border = 0;
            skip.text_normal = nk_rgb(100, 100, 100);
            skip.text_hover  = nk_rgb(150, 150, 150);
            skip.text_active = nk_rgb(100, 100, 100);
            if (nk_button_label_styled(ctx, &skip, "Skip")) {
                st->show_wizard = 0;
                wizard_write_setup_done();
            }
        } else {
            nk_spacing(ctx, 1);
        }

        /* Center spacer */
        nk_spacing(ctx, 1);

        /* Back button */
        if (st->wizard_step > 0 && !st->wizard_building) {
            struct nk_style_button back = ctx->style.button;
            back.rounding = 6;
            if (nk_button_label_styled(ctx, &back, "Back"))
                st->wizard_step--;
        } else {
            nk_spacing(ctx, 1);
        }

        /* Next / Start button */
        if (st->wizard_step < 3) {
            int can_next = 1;
            if (st->wizard_step == 1 && !st->index_found) can_next = 0;
            if (st->wizard_building) can_next = 0;

            if (can_next) {
                struct nk_style_button green = ctx->style.button;
                green.normal  = nk_style_item_color(nk_rgb(34, 139, 34));
                green.hover   = nk_style_item_color(nk_rgb(40, 170, 40));
                green.active  = nk_style_item_color(nk_rgb(25, 110, 25));
                green.text_normal = nk_rgb(255, 255, 255);
                green.text_hover  = nk_rgb(255, 255, 255);
                green.text_active = nk_rgb(255, 255, 255);
                green.rounding = 6;
                if (nk_button_label_styled(ctx, &green, "Next ->"))
                    st->wizard_step++;
            } else {
                struct nk_style_button grey = ctx->style.button;
                grey.normal = nk_style_item_color(nk_rgb(60, 60, 65));
                grey.hover  = nk_style_item_color(nk_rgb(60, 60, 65));
                grey.text_normal = nk_rgb(100, 100, 100);
                grey.text_hover  = nk_rgb(100, 100, 100);
                grey.rounding = 6;
                nk_button_label_styled(ctx, &grey, "Next ->");
            }
        } else {
            struct nk_style_button green = ctx->style.button;
            green.normal  = nk_style_item_color(nk_rgb(34, 139, 34));
            green.hover   = nk_style_item_color(nk_rgb(40, 170, 40));
            green.active  = nk_style_item_color(nk_rgb(25, 110, 25));
            green.text_normal = nk_rgb(255, 255, 255);
            green.text_hover  = nk_rgb(255, 255, 255);
            green.text_active = nk_rgb(255, 255, 255);
            green.rounding = 6;
            if (nk_button_label_styled(ctx, &green, "Start")) {
                st->show_wizard = 0;
                wizard_write_setup_done();
            }
        }
    }
}

/* ================================================================== */
/* Draw the full GUI layout                                            */
/* ================================================================== */
static void draw_gui(struct nk_context *ctx, gui_state_t *st,
                     int win_w, int win_h)
{
    analysis_context_t *analysis = &st->analysis;

    if (!nk_begin(ctx, "SpeciesID",
                  nk_rect(0, 0, (float)win_w, (float)win_h),
                  NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
        nk_end(ctx);
        return;
    }

    /* Show wizard instead of normal layout */
    if (st->show_wizard) {
        /* Title */
        nk_layout_row_dynamic(ctx, ROW_TITLE, 1);
        nk_label(ctx, "SpeciesID - Setup", NK_TEXT_CENTERED);

        nk_layout_row_dynamic(ctx, 2, 1);
        nk_spacing(ctx, 1);

        /* Centered wizard group */
        float margin = ((float)win_w - 540.0f) * 0.5f;
        if (margin < 20.0f) margin = 20.0f;
        float grp_w = (float)win_w - margin * 2.0f;
        float cols[] = { margin, grp_w, margin };
        nk_layout_row(ctx, NK_STATIC,
                       (float)(win_h - ROW_TITLE - 24), 3, cols);
        nk_spacing(ctx, 1);  /* left margin */
        if (nk_group_begin(ctx, "wizard_group",
                           NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
            draw_wizard_content(ctx, st, grp_w);
            nk_group_end(ctx);
        }
        nk_spacing(ctx, 1);  /* right margin */
        nk_end(ctx);
        return;
    }

    /* Title bar */
    nk_layout_row_dynamic(ctx, ROW_TITLE, 1);
    nk_label(ctx, "SpeciesID - Food DNA Authentication", NK_TEXT_CENTERED);

    nk_layout_row_dynamic(ctx, 2, 1);
    nk_spacing(ctx, 1);

    /* Two-column layout */
    float col_widths[] = { (float)LEFT_W, (float)(win_w - LEFT_W - 20) };
    nk_layout_row(ctx, NK_STATIC, (float)(win_h - ROW_TITLE - 24), 2, col_widths);

    /* ============================================================== */
    /* LEFT PANEL                                                      */
    /* ============================================================== */
    if (nk_group_begin(ctx, "input_panel", NK_WINDOW_BORDER)) {

        /* ---- Database update notification banner ---- */
        if (st->update_available && !st->update_dismissed &&
            !st->update_in_progress) {
            /* Blue info banner */
            nk_layout_row_dynamic(ctx, 28, 1);
            {
                struct nk_rect bx;
                nk_widget(&bx, ctx);
                struct nk_command_buffer *c = nk_window_get_canvas(ctx);
                nk_fill_rect(c, bx, 6, nk_rgba(30, 100, 180, 35));
                nk_stroke_rect(c, bx, 6, 1.0f, nk_rgba(60, 140, 220, 80));
                char ver_msg[128];
                snprintf(ver_msg, sizeof(ver_msg),
                         "Database v%u available (current: v%u)",
                         st->update_ctx.manifest.version,
                         st->update_ctx.local_version);
                nk_draw_text(c,
                    nk_rect(bx.x + 10, bx.y, bx.w - 20, bx.h),
                    ver_msg, (int)strlen(ver_msg),
                    ctx->style.font, nk_rgba(0,0,0,0),
                    nk_rgb(100, 180, 255));
            }
            nk_layout_row_dynamic(ctx, ROW_BTN, 2);
            {
                struct nk_style_button upd_btn = ctx->style.button;
                upd_btn.normal = nk_style_item_color(nk_rgb(30, 100, 180));
                upd_btn.hover  = nk_style_item_color(nk_rgb(40, 120, 210));
                upd_btn.active = nk_style_item_color(nk_rgb(20, 80, 150));
                upd_btn.text_normal = nk_rgb(255, 255, 255);
                upd_btn.text_hover  = nk_rgb(255, 255, 255);
                upd_btn.text_active = nk_rgb(255, 255, 255);
                upd_btn.rounding = 6;
                if (nk_button_label_styled(ctx, &upd_btn, "Update Now")) {
                    st->update_in_progress = 1;
                    SDL_DetachThread(
                        SDL_CreateThread(update_download_worker, "update_dl", st));
                }
            }
            if (nk_button_label(ctx, "Dismiss"))
                st->update_dismissed = 1;
            nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
            nk_spacing(ctx, 1);
        }
        if (st->update_in_progress) {
            nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
            const char *upd_text;
            int us = st->update_ctx.status;
            if (us == SID_UPDATE_DOWNLOADING)      upd_text = "Downloading...";
            else if (us == SID_UPDATE_VERIFYING)    upd_text = "Verifying...";
            else if (us == SID_UPDATE_INSTALLING)   upd_text = "Installing...";
            else if (us == SID_UPDATE_DONE)         upd_text = "Update complete!";
            else if (us == SID_UPDATE_ERROR)        upd_text = st->update_ctx.error_msg;
            else                                   upd_text = "Updating...";
            struct nk_color upd_col = (us == SID_UPDATE_DONE)
                ? nk_rgb(34, 200, 34)
                : (us == SID_UPDATE_ERROR)
                    ? nk_rgb(220, 100, 100)
                    : nk_rgb(100, 180, 255);
            nk_label_colored(ctx, upd_text, NK_TEXT_LEFT, upd_col);
            nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
            nk_spacing(ctx, 1);
        }

        nk_layout_row_dynamic(ctx, ROW_LABEL, 1);
        nk_label(ctx, "DNA Sample Files:", NK_TEXT_LEFT);

        /* Sample list display */
        if (analysis->n_samples > 0) {
            int show = analysis->n_samples > 6 ? 6 : analysis->n_samples;
            for (int i = 0; i < show; i++) {
                nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
                char label[300];
                if (analysis->samples[i].n_files == 2)
                    snprintf(label, sizeof(label), "%s (R1+R2)",
                             analysis->samples[i].sample_name);
                else
                    snprintf(label, sizeof(label), "%s",
                             analysis->samples[i].sample_name);
                nk_label(ctx, label, NK_TEXT_LEFT);
            }
            if (analysis->n_samples > 6) {
                nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
                char more[32];
                snprintf(more, sizeof(more), "... +%d more",
                         analysis->n_samples - 6);
                nk_label(ctx, more, NK_TEXT_LEFT);
            }

            /* Total file size & sample count */
            {
                mem_estimate_t est = analysis_estimate_memory(analysis);
                char sz_buf[64];
                format_bytes(est.total_file_bytes, sz_buf, sizeof(sz_buf));
                nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
                char info[128];
                snprintf(info, sizeof(info),
                         "%d sample%s (%d file%s, %s)",
                         analysis->n_samples,
                         analysis->n_samples > 1 ? "s" : "",
                         analysis->n_fastq_files,
                         analysis->n_fastq_files > 1 ? "s" : "",
                         sz_buf);
                nk_label(ctx, info, NK_TEXT_LEFT);

                /* RAM estimate */
                nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
                char ram_info[128];
                snprintf(ram_info, sizeof(ram_info),
                         "Est. reads: %dK, RAM: %d MB",
                         est.estimated_reads / 1000,
                         est.estimated_ram_mb);
                nk_label(ctx, ram_info, NK_TEXT_LEFT);

                /* RAM warning */
                if (est.estimated_ram_mb > 1024) {
                    nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
                    nk_label_colored(ctx,
                        "Warning: >1 GB RAM estimated",
                        NK_TEXT_LEFT, nk_rgb(255, 200, 50));

                    nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
                    nk_checkbox_label(ctx, "Subsample to 500K reads/sample",
                                     &analysis->subsample_enabled);
                }
            }
        } else {
            nk_layout_row_dynamic(ctx, ROW_LABEL, 1);
            nk_label(ctx, "(drop files or click Choose Files)",
                     NK_TEXT_LEFT);
        }

        nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
        nk_spacing(ctx, 1);

        /* Browse + Clear buttons */
        nk_layout_row_dynamic(ctx, ROW_BTN, 2);
        if (nk_button_label(ctx, "Choose Files...")) {
            open_file_dialog(st);
        }
        {
            int has_files = analysis->n_fastq_files > 0;
            if (has_files && nk_button_label(ctx, "Clear Files")) {
                analysis->n_fastq_files = 0;
                analysis->n_samples = 0;
            } else if (!has_files) {
                /* Draw disabled clear button */
                struct nk_style_button grey = ctx->style.button;
                grey.normal = nk_style_item_color(nk_rgb(50, 50, 50));
                grey.hover  = nk_style_item_color(nk_rgb(50, 50, 50));
                grey.text_normal = nk_rgb(100, 100, 100);
                grey.text_hover  = nk_rgb(100, 100, 100);
                nk_button_label_styled(ctx, &grey, "Clear Files");
            }
        }

        nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
        nk_spacing(ctx, 1);

        /* Analyze button */
        nk_layout_row_dynamic(ctx, ROW_BTN, 1);
        {
            int can_run = (analysis->n_fastq_files > 0 &&
                          (analysis->state == ANALYSIS_IDLE ||
                           analysis->state == ANALYSIS_DONE ||
                           analysis->state == ANALYSIS_ERROR) &&
                          st->index_path[0]);
            if (can_run) {
                struct nk_style_button green = ctx->style.button;
                green.normal  = nk_style_item_color(nk_rgb(34, 139, 34));
                green.hover   = nk_style_item_color(nk_rgb(0, 180, 0));
                green.active  = nk_style_item_color(nk_rgb(0, 140, 0));
                green.text_normal  = nk_rgb(255, 255, 255);
                green.text_hover   = nk_rgb(255, 255, 255);
                green.text_active  = nk_rgb(255, 255, 255);
                if (nk_button_label_styled(ctx, &green, "Run Analysis")) {
                    snprintf(analysis->index_path,
                             sizeof(analysis->index_path), "%s",
                             st->index_path);
                    analysis_start(analysis);
                }
            } else {
                struct nk_style_button grey = ctx->style.button;
                grey.normal = nk_style_item_color(nk_rgb(80, 80, 80));
                grey.hover  = nk_style_item_color(nk_rgb(80, 80, 80));
                grey.active = nk_style_item_color(nk_rgb(80, 80, 80));
                grey.text_normal = nk_rgb(140, 140, 140);
                grey.text_hover  = nk_rgb(140, 140, 140);
                grey.text_active = nk_rgb(140, 140, 140);
                nk_button_label_styled(ctx, &grey, "Run Analysis");
            }
        }

        /* Status line — friendly labels with per-sample progress */
        nk_layout_row_dynamic(ctx, ROW_LABEL, 1);
        {
            static char status_buf[128];
            const char *status_text;
            int si = analysis->progress_sample_idx;
            int ns = analysis->n_samples;
            const char *sname = (ns > 0 && si < ns)
                ? analysis->samples[si].sample_name : "";

            switch (analysis->state) {
                case ANALYSIS_IDLE:              status_text = "Ready"; break;
                case ANALYSIS_LOADING_INDEX:     status_text = "Preparing..."; break;
                case ANALYSIS_READING_FASTQ:
                    if (ns > 1)
                        snprintf(status_buf, sizeof(status_buf),
                                 "Sample %d/%d: %s (%d reads)...",
                                 si + 1, ns, sname,
                                 analysis->progress_reads);
                    else
                        snprintf(status_buf, sizeof(status_buf),
                                 "Reading sample (%d reads)...",
                                 analysis->progress_reads);
                    status_text = status_buf;
                    break;
                case ANALYSIS_CLASSIFYING:
                    if (ns > 1)
                        snprintf(status_buf, sizeof(status_buf),
                                 "Sample %d/%d: Identifying species...",
                                 si + 1, ns);
                    else
                        snprintf(status_buf, sizeof(status_buf),
                                 "Identifying species...");
                    status_text = status_buf;
                    break;
                case ANALYSIS_RUNNING_EM:
                    if (ns > 1)
                        snprintf(status_buf, sizeof(status_buf),
                                 "Sample %d/%d: Calculating amounts...",
                                 si + 1, ns);
                    else
                        snprintf(status_buf, sizeof(status_buf),
                                 "Calculating amounts...");
                    status_text = status_buf;
                    break;
                case ANALYSIS_GENERATING_REPORT:
                    if (ns > 1)
                        snprintf(status_buf, sizeof(status_buf),
                                 "Sample %d/%d: Generating report...",
                                 si + 1, ns);
                    else
                        snprintf(status_buf, sizeof(status_buf),
                                 "Generating report...");
                    status_text = status_buf;
                    break;
                case ANALYSIS_DONE:              status_text = "Analysis complete"; break;
                case ANALYSIS_ERROR:             status_text = "Error occurred"; break;
                default:                         status_text = ""; break;
            }
            nk_label(ctx, status_text, NK_TEXT_LEFT);
        }

        /* Progress bar */
        if (analysis->state > ANALYSIS_IDLE &&
            analysis->state < ANALYSIS_DONE) {
            nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
            nk_size pv = 0;
            switch (analysis->state) {
                case ANALYSIS_LOADING_INDEX:     pv = 10; break;
                case ANALYSIS_READING_FASTQ:     pv = 25; break;
                case ANALYSIS_CLASSIFYING:       pv = 50; break;
                case ANALYSIS_RUNNING_EM:        pv = 75; break;
                case ANALYSIS_GENERATING_REPORT: pv = 90; break;
                default: break;
            }
            nk_progress(ctx, &pv, 100, NK_FIXED);
        }

        /* Stacked species bar + legend (after results) */
        {
            halal_report_t *sel_rpt = NULL;
            if (analysis->state == ANALYSIS_DONE &&
                analysis->reports &&
                analysis->selected_sample < analysis->n_reports)
                sel_rpt = analysis->reports[analysis->selected_sample];
            if (sel_rpt) {
                nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
                nk_spacing(ctx, 1);
                nk_layout_row_dynamic(ctx, ROW_LABEL, 1);
                nk_label(ctx, "Sample Composition:", NK_TEXT_LEFT);
                draw_species_bar(ctx, sel_rpt);

                /* Legend: show species names next to color blocks */
                nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
                nk_spacing(ctx, 1);
                for (int i = 0; i < sel_rpt->n_species; i++) {
                    species_report_t *sp = &sel_rpt->species[i];
                    if (sp->weight_pct < 0.5) continue;
                    nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
                    char legend[128];
                    snprintf(legend, sizeof(legend), "  %s: %.1f%%",
                             friendly_species_name(sp->species_id),
                             sp->weight_pct);
                    nk_label_colored(ctx, legend, NK_TEXT_LEFT,
                                     status_color(sp->halal_status));
                }
            }
        }

        nk_group_end(ctx);
    }

    /* ============================================================== */
    /* RIGHT PANEL                                                     */
    /* ============================================================== */
    if (nk_group_begin(ctx, "results_panel", NK_WINDOW_BORDER)) {

        /* Tab toggle: Results / Database */
        nk_layout_row_dynamic(ctx, ROW_LABEL, 2);
        if (nk_option_label(ctx, "Results", st->right_panel_mode == 0))
            st->right_panel_mode = 0;
        if (nk_option_label(ctx, "Database", st->right_panel_mode == 1))
            st->right_panel_mode = 1;

        nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
        nk_spacing(ctx, 1);

        if (st->right_panel_mode == 1) {
            /* --- Database viewer --- */
            draw_database_panel(ctx, st->db_info);

        } else if (analysis->reports && analysis->state == ANALYSIS_DONE) {
            /* --- Sample selector tabs (if multiple samples) --- */
            if (analysis->n_reports > 1) {
                /* Show up to 8 sample tabs per row */
                int tabs_per_row = analysis->n_reports < 8
                    ? analysis->n_reports : 8;
                nk_layout_row_dynamic(ctx, ROW_BTN, tabs_per_row);
                for (int i = 0; i < analysis->n_reports; i++) {
                    halal_report_t *sr = analysis->reports[i];
                    const char *tab_label = sr ? sr->sample_id : "?";
                    if (analysis->selected_sample == i) {
                        /* Active tab — highlight */
                        struct nk_style_button active = ctx->style.button;
                        active.normal = nk_style_item_color(nk_rgb(60, 120, 60));
                        active.hover  = nk_style_item_color(nk_rgb(70, 140, 70));
                        active.text_normal = nk_rgb(255, 255, 255);
                        active.text_hover  = nk_rgb(255, 255, 255);
                        if (nk_button_label_styled(ctx, &active, tab_label))
                            analysis->selected_sample = i;
                    } else {
                        if (nk_button_label(ctx, tab_label))
                            analysis->selected_sample = i;
                    }
                }
                nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
                nk_spacing(ctx, 1);
            }

            /* --- Results for selected sample --- */
            halal_report_t *rpt = NULL;
            if (analysis->selected_sample < analysis->n_reports)
                rpt = analysis->reports[analysis->selected_sample];

            if (rpt) {
                /* Verdict — large, descriptive */
                nk_layout_row_dynamic(ctx, 44, 1);
                {
                    struct nk_color vc = verdict_color(rpt->verdict);
                    nk_label_colored(ctx, friendly_verdict(rpt->verdict),
                                     NK_TEXT_CENTERED, vc);
                }

                nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
                nk_spacing(ctx, 1);

                /* Threshold slider + horizontal bar chart */
                nk_layout_row_dynamic(ctx, ROW_LABEL, 1);
                {
                    char slider_label[64];
                    snprintf(slider_label, sizeof(slider_label),
                             "Show species above: %.1f%%",
                             st->min_display_pct);
                    nk_label(ctx, slider_label, NK_TEXT_LEFT);
                }
                nk_layout_row_dynamic(ctx, ROW_SMALL, 1);
                nk_slider_float(ctx, 0.0f, &st->min_display_pct, 5.0f, 0.1f);

                nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
                nk_spacing(ctx, 1);

                nk_layout_row_dynamic(ctx, ROW_LABEL, 1);
                nk_label(ctx, "Species Proportions", NK_TEXT_LEFT);

                float right_w = col_widths[1] - 16.0f;
                draw_horizontal_bars(ctx, rpt, right_w,
                                      st->min_display_pct);

                nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
                nk_spacing(ctx, 1);

                /* Column headers — friendly names, 4 columns */
                nk_layout_row_dynamic(ctx, ROW_TABLE, 4);
                nk_label(ctx, "Animal",  NK_TEXT_LEFT);
                nk_label(ctx, "Status",  NK_TEXT_CENTERED);
                nk_label(ctx, "Amount",  NK_TEXT_RIGHT);
                nk_label(ctx, "Range",   NK_TEXT_RIGHT);

                /* Species rows — show all detected */
                for (int s = 0; s < rpt->n_species; s++) {
                    species_report_t *sp = &rpt->species[s];
                    if (sp->weight_pct < 0.001 && sp->read_pct < 0.001)
                        continue;

                    nk_layout_row_dynamic(ctx, ROW_TABLE, 4);

                    /* Common name */
                    nk_label(ctx, friendly_species_name(sp->species_id),
                             NK_TEXT_LEFT);

                    /* Status (colour-coded) */
                    nk_label_colored(ctx, friendly_status(sp->halal_status),
                                     NK_TEXT_CENTERED,
                                     status_color(sp->halal_status));

                    /* Amount */
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%.1f%%", sp->weight_pct);
                    nk_label(ctx, buf, NK_TEXT_RIGHT);

                    /* Range */
                    snprintf(buf, sizeof(buf), "%.1f-%.1f%%",
                             sp->ci_lo, sp->ci_hi);
                    nk_label(ctx, buf, NK_TEXT_RIGHT);
                }

                /* Summary — plain English */
                nk_layout_row_dynamic(ctx, ROW_SPACE, 1);
                nk_spacing(ctx, 1);

                nk_layout_row_dynamic(ctx, ROW_LABEL, 1);
                {
                    char meta[256];
                    snprintf(meta, sizeof(meta),
                             "%d DNA fragments analyzed",
                             rpt->total_reads);
                    nk_label(ctx, meta, NK_TEXT_LEFT);
                }

                if (rpt->cross_marker_agreement > 0) {
                    nk_layout_row_dynamic(ctx, ROW_LABEL, 1);
                    char conf[128];
                    snprintf(conf, sizeof(conf), "Confidence: %s",
                             confidence_label(rpt->cross_marker_agreement));
                    nk_label(ctx, conf, NK_TEXT_LEFT);
                }
            }

        } else if (analysis->state == ANALYSIS_IDLE) {
            nk_layout_row_dynamic(ctx, 50, 1);
            nk_label_wrap(ctx,
                "Choose DNA sample file(s) and click "
                "Run Analysis to check halal status.");
        } else if (analysis->state == ANALYSIS_ERROR) {
            nk_layout_row_dynamic(ctx, 50, 1);
            nk_label_colored_wrap(ctx, analysis->error_msg,
                                  nk_rgb(220, 60, 60));
        } else {
            nk_layout_row_dynamic(ctx, 50, 1);
            nk_label_wrap(ctx, "Analyzing your sample, please wait...");
        }

        nk_group_end(ctx);
    }

    nk_end(ctx);
}

/* ================================================================== */
/* Main                                                                */
/* ================================================================== */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");

    SDL_Window *window = SDL_CreateWindow(
        "SpeciesID - Food DNA Authentication",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* --- Compute DPI scale and set renderer scale -------------------- */
    float dpi_scale = 1.0f;
    {
        int render_w, window_w;
        SDL_GetRendererOutputSize(renderer, &render_w, NULL);
        SDL_GetWindowSize(window, &window_w, NULL);
        if (window_w > 0)
            dpi_scale = (float)render_w / (float)window_w;
        if (dpi_scale < 1.0f) dpi_scale = 1.0f;
    }
    SDL_RenderSetScale(renderer, dpi_scale, dpi_scale);

    /* --- Nuklear init ---------------------------------------------- */
    struct nk_context *ctx = nk_sdl_init(window, renderer);
    {
        struct nk_font_atlas *atlas;
        struct nk_font_config cfg = nk_font_config(0);
        struct nk_font *font = NULL;

        cfg.oversample_h = 3;
        cfg.oversample_v = 2;

        float font_size = 18.0f * dpi_scale;

        nk_sdl_font_stash_begin(&atlas);

        static const char *font_paths[] = {
            "/System/Library/Fonts/Supplemental/Arial.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            "C:\\Windows\\Fonts\\arial.ttf",
            "C:\\Windows\\Fonts\\segoeui.ttf",
            NULL
        };
        for (const char **p = font_paths; *p; p++) {
            font = nk_font_atlas_add_from_file(atlas, *p, font_size, &cfg);
            if (font) break;
        }
        if (!font)
            font = nk_font_atlas_add_default(atlas, font_size, &cfg);

        nk_sdl_font_stash_end();

        font->handle.height /= dpi_scale;
        nk_style_set_font(ctx, &font->handle);
    }

    /* Dark theme */
    {
        struct nk_color table[NK_COLOR_COUNT];
        table[NK_COLOR_TEXT]                    = nk_rgba(210, 210, 210, 255);
        table[NK_COLOR_WINDOW]                  = nk_rgba(35, 35, 38, 255);
        table[NK_COLOR_HEADER]                  = nk_rgba(50, 50, 55, 255);
        table[NK_COLOR_BORDER]                  = nk_rgba(65, 65, 70, 255);
        table[NK_COLOR_BUTTON]                  = nk_rgba(60, 60, 65, 255);
        table[NK_COLOR_BUTTON_HOVER]            = nk_rgba(75, 75, 80, 255);
        table[NK_COLOR_BUTTON_ACTIVE]           = nk_rgba(50, 50, 55, 255);
        table[NK_COLOR_TOGGLE]                  = nk_rgba(50, 50, 55, 255);
        table[NK_COLOR_TOGGLE_HOVER]            = nk_rgba(55, 55, 60, 255);
        table[NK_COLOR_TOGGLE_CURSOR]           = nk_rgba(44, 160, 44, 255);
        table[NK_COLOR_SELECT]                  = nk_rgba(50, 50, 55, 255);
        table[NK_COLOR_SELECT_ACTIVE]           = nk_rgba(44, 160, 44, 255);
        table[NK_COLOR_SLIDER]                  = nk_rgba(50, 50, 55, 255);
        table[NK_COLOR_SLIDER_CURSOR]           = nk_rgba(44, 160, 44, 255);
        table[NK_COLOR_SLIDER_CURSOR_HOVER]     = nk_rgba(60, 180, 60, 255);
        table[NK_COLOR_SLIDER_CURSOR_ACTIVE]    = nk_rgba(34, 140, 34, 255);
        table[NK_COLOR_PROPERTY]                = nk_rgba(50, 50, 55, 255);
        table[NK_COLOR_EDIT]                    = nk_rgba(45, 45, 50, 255);
        table[NK_COLOR_EDIT_CURSOR]             = nk_rgba(210, 210, 210, 255);
        table[NK_COLOR_COMBO]                   = nk_rgba(50, 50, 55, 255);
        table[NK_COLOR_CHART]                   = nk_rgba(50, 50, 55, 255);
        table[NK_COLOR_CHART_COLOR]             = nk_rgba(44, 160, 44, 255);
        table[NK_COLOR_CHART_COLOR_HIGHLIGHT]   = nk_rgba(255, 0, 0, 255);
        table[NK_COLOR_SCROLLBAR]               = nk_rgba(40, 40, 45, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR]        = nk_rgba(60, 60, 65, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR_HOVER]  = nk_rgba(75, 75, 80, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(55, 55, 60, 255);
        table[NK_COLOR_TAB_HEADER]              = nk_rgba(50, 50, 55, 255);
        nk_style_from_table(ctx, table);
    }

    /* --- App state ------------------------------------------------- */
    gui_state_t state;
    memset(&state, 0, sizeof(state));
    analysis_init(&state.analysis);
    state.min_display_pct = 0.1f;

    find_default_index(state.index_path, sizeof(state.index_path));
    state.index_found = (state.index_path[0] != '\0') ? 1 : 0;

    /* Load database info for database viewer */
    state.db_info = load_db_info(state.index_path);

    /* Initialise update context */
    sid_update_init(&state.update_ctx);
    if (state.index_path[0])
        snprintf(state.update_ctx.index_path,
                 sizeof(state.update_ctx.index_path), "%s", state.index_path);

    /* First-launch wizard: show if no setup_done marker or no index */
    if (!wizard_setup_done_exists() || !state.index_found) {
        state.show_wizard = 1;
        state.wizard_step = 0;
    }

    /* Auto-check for database updates on startup (if index exists and
     * not showing wizard — don't distract the first-launch flow) */
    if (state.index_found && !state.show_wizard) {
        state.update_checking = 1;
        SDL_DetachThread(
            SDL_CreateThread(update_check_worker, "update_check", &state));
    }

    /* --- Main loop ------------------------------------------------- */
    int running = 1;
    while (running) {
        SDL_Event evt;
        nk_input_begin(ctx);
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_QUIT) {
                running = 0;
            }
            if (evt.type == SDL_DROPFILE) {
                /* Accumulate dropped files */
                if (state.analysis.n_fastq_files < 32) {
                    snprintf(state.analysis.fastq_paths[state.analysis.n_fastq_files],
                             1024, "%s", evt.drop.file);
                    state.analysis.n_fastq_files++;
                    detect_samples(&state.analysis);
                }
                SDL_free(evt.drop.file);
            }
            nk_sdl_handle_event(&evt);
        }
        nk_input_end(ctx);

        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);

        draw_gui(ctx, &state, win_w, win_h);

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
        nk_sdl_render(NK_ANTI_ALIASING_ON);
        SDL_RenderPresent(renderer);
    }

    analysis_cleanup(&state.analysis);
    if (state.db_info) refdb_destroy(state.db_info);
    nk_sdl_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
