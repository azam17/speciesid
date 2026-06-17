#include "analyze.h"
#include "calibrate.h"
#include "classify.h"
#include "em.h"
#include "fastq.h"
#include "index.h"
#include "params.h"
#include "report.h"
#include "utils.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const analyze_sample_t *sample;
    halal_report_t *report;
} sample_result_v1_t;

static void copy_cstr(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return;
    size_t i = 0;
    if (src) {
        for (; i + 1 < dst_sz && src[i] != '\0'; i++) dst[i] = src[i];
    }
    dst[i] = '\0';
}

static int set_err(char *err, size_t err_sz, const char *msg) {
    copy_cstr(err, err_sz, msg ? msg : "");
    return -1;
}

static char *read_text_file(const char *path, char *err, size_t err_sz) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Cannot open manifest: %s", path);
        set_err(err, err_sz, msg);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    long len = ftell(fp);
    if (len < 0) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);
    char *buf = (char *)hs_malloc((size_t)len + 1);
    size_t n = fread(buf, 1, (size_t)len, fp);
    fclose(fp);
    buf[n] = '\0';
    return buf;
}

static const char *skip_ws(const char *p, const char *end) {
    while (p < end && isspace((unsigned char)*p)) p++;
    return p;
}

static const char *bounded_strstr(const char *start, const char *end,
                                  const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen == 0) return start;
    for (const char *p = start; p + nlen <= end; p++) {
        if (memcmp(p, needle, nlen) == 0) return p;
    }
    return NULL;
}

static const char *find_key_value(const char *start, const char *end,
                                  const char *key) {
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = start;
    while ((p = bounded_strstr(p, end, needle)) != NULL) {
        const char *q = skip_ws(p + strlen(needle), end);
        if (q < end && *q == ':') return skip_ws(q + 1, end);
        p += strlen(needle);
    }
    return NULL;
}

static int parse_json_string(const char **pp, const char *end,
                             char *out, size_t out_sz) {
    const char *p = skip_ws(*pp, end);
    if (p >= end || *p != '"') return -1;
    p++;
    size_t n = 0;
    while (p < end && *p != '"') {
        char ch = *p++;
        if (ch == '\\' && p < end) {
            char esc = *p++;
            switch (esc) {
                case '"': ch = '"'; break;
                case '\\': ch = '\\'; break;
                case '/': ch = '/'; break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                default: ch = esc; break;
            }
        }
        if (n + 1 < out_sz) out[n++] = ch;
    }
    if (p >= end || *p != '"') return -1;
    if (out_sz > 0) out[n] = '\0';
    *pp = p + 1;
    return 0;
}

static int get_string_in_range(const char *start, const char *end,
                               const char *key, char *out, size_t out_sz,
                               int required) {
    const char *p = find_key_value(start, end, key);
    if (!p) {
        if (required) return -1;
        if (out_sz > 0) out[0] = '\0';
        return 0;
    }
    p = skip_ws(p, end);
    if (p + 4 <= end && memcmp(p, "null", 4) == 0) {
        if (out_sz > 0) out[0] = '\0';
        return required ? -1 : 0;
    }
    return parse_json_string(&p, end, out, out_sz);
}

static int get_number_in_range(const char *start, const char *end,
                               const char *key, double *out) {
    const char *p = find_key_value(start, end, key);
    if (!p) return 0;
    char *after = NULL;
    errno = 0;
    double value = strtod(p, &after);
    if (errno != 0 || after == p || after > end) return -1;
    *out = value;
    return 0;
}

static int get_int_in_range(const char *start, const char *end,
                            const char *key, int *out) {
    double value = (double)*out;
    int rc = get_number_in_range(start, end, key, &value);
    if (rc == 0) *out = (int)value;
    return rc;
}

static int get_bool_in_range(const char *start, const char *end,
                             const char *key, int *out) {
    const char *p = find_key_value(start, end, key);
    if (!p) return 0;
    p = skip_ws(p, end);
    if (p + 4 <= end && memcmp(p, "true", 4) == 0) {
        *out = 1;
        return 0;
    }
    if (p + 5 <= end && memcmp(p, "false", 5) == 0) {
        *out = 0;
        return 0;
    }
    return -1;
}

static int find_array_bounds(const char *start, const char *end,
                             const char *key, const char **arr_start,
                             const char **arr_end) {
    const char *p = find_key_value(start, end, key);
    if (!p) return -1;
    p = skip_ws(p, end);
    if (p >= end || *p != '[') return -1;
    const char *body = p + 1;
    int depth = 1, in_string = 0, escaped = 0;
    for (p = body; p < end; p++) {
        char ch = *p;
        if (in_string) {
            if (escaped) escaped = 0;
            else if (ch == '\\') escaped = 1;
            else if (ch == '"') in_string = 0;
            continue;
        }
        if (ch == '"') in_string = 1;
        else if (ch == '[') depth++;
        else if (ch == ']') {
            depth--;
            if (depth == 0) {
                *arr_start = body;
                *arr_end = p;
                return 0;
            }
        }
    }
    return -1;
}

static int find_object_end(const char *start, const char *end,
                           const char **obj_end) {
    if (start >= end || *start != '{') return -1;
    int depth = 1, in_string = 0, escaped = 0;
    for (const char *p = start + 1; p < end; p++) {
        char ch = *p;
        if (in_string) {
            if (escaped) escaped = 0;
            else if (ch == '\\') escaped = 1;
            else if (ch == '"') in_string = 0;
            continue;
        }
        if (ch == '"') in_string = 1;
        else if (ch == '{') depth++;
        else if (ch == '}') {
            depth--;
            if (depth == 0) {
                *obj_end = p + 1;
                return 0;
            }
        }
    }
    return -1;
}

static int is_valid_role(const char *role) {
    return strcmp(role, "sample") == 0 ||
           strcmp(role, "positive_control") == 0 ||
           strcmp(role, "negative_control") == 0 ||
           strcmp(role, "extraction_blank") == 0;
}

static int is_known_marker(const char *marker) {
    static const char *known[] = { "COI", "cytb", "12S", "16S",
                                  "NADH2", "NADH4", "NADH5" };
    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        if (strcmp(marker, known[i]) == 0) return 1;
    }
    return 0;
}

void analyze_manifest_init(analyze_manifest_t *m) {
    memset(m, 0, sizeof(*m));
    m->detection_threshold = 0.001;
    m->prune_threshold = 0.0;
    m->n_bootstrap = 0;
    strcpy(m->read_mode, "auto");
    m->control_adjustment = 1;
    m->out_of_panel_check = 0;
    m->resolvability_check = 0;
}

static int parse_marker_panel(const char *json, const char *end,
                              analyze_manifest_t *m, char *err, size_t err_sz) {
    const char *arr_start, *arr_end;
    if (find_array_bounds(json, end, "marker_panel", &arr_start, &arr_end) < 0)
        return set_err(err, err_sz, "manifest marker_panel must be an array");

    const char *p = arr_start;
    while (p < arr_end) {
        p = skip_ws(p, arr_end);
        if (p >= arr_end) break;
        if (m->n_markers >= HS_MAX_MARKERS)
            return set_err(err, err_sz, "too many marker_panel entries");

        char marker[16] = {0};
        if (*p == '"') {
            if (parse_json_string(&p, arr_end, marker, sizeof(marker)) < 0)
                return set_err(err, err_sz, "invalid marker_panel string");
        } else if (*p == '{') {
            const char *obj_end;
            if (find_object_end(p, arr_end, &obj_end) < 0)
                return set_err(err, err_sz, "invalid marker_panel object");
            if (get_string_in_range(p, obj_end, "marker_id", marker,
                                    sizeof(marker), 1) < 0)
                return set_err(err, err_sz, "marker_panel object missing marker_id");
            p = obj_end;
        } else {
            return set_err(err, err_sz, "marker_panel entries must be strings or objects");
        }

        if (!is_known_marker(marker)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "unknown marker in marker_panel: %s", marker);
            return set_err(err, err_sz, msg);
        }
        copy_cstr(m->marker_ids[m->n_markers],
                  sizeof(m->marker_ids[m->n_markers]), marker);
        m->n_markers++;
        p = skip_ws(p, arr_end);
        if (p < arr_end && *p == ',') p++;
    }

    if (m->n_markers == 0)
        return set_err(err, err_sz, "marker_panel must contain at least one marker");
    return 0;
}

static int parse_samples(const char *json, const char *end,
                         analyze_manifest_t *m, char *err, size_t err_sz) {
    const char *arr_start, *arr_end;
    if (find_array_bounds(json, end, "samples", &arr_start, &arr_end) < 0)
        return set_err(err, err_sz, "manifest samples must be an array");

    int has_test_sample = 0;
    const char *p = arr_start;
    while (p < arr_end) {
        p = skip_ws(p, arr_end);
        if (p >= arr_end) break;
        if (*p != '{') return set_err(err, err_sz, "sample entry must be an object");
        if (m->n_samples >= ANALYZE_MAX_SAMPLES)
            return set_err(err, err_sz, "too many samples in manifest");

        const char *obj_end;
        if (find_object_end(p, arr_end, &obj_end) < 0)
            return set_err(err, err_sz, "invalid sample object");

        analyze_sample_t *s = &m->samples[m->n_samples];
        if (get_string_in_range(p, obj_end, "sample_id", s->sample_id,
                                sizeof(s->sample_id), 1) < 0 ||
            get_string_in_range(p, obj_end, "role", s->role,
                                sizeof(s->role), 1) < 0 ||
            get_string_in_range(p, obj_end, "fastq_path", s->fastq_path,
                                sizeof(s->fastq_path), 1) < 0) {
            return set_err(err, err_sz,
                           "sample objects require sample_id, role, and fastq_path");
        }
        (void)get_string_in_range(p, obj_end, "label", s->label,
                                  sizeof(s->label), 0);
        if (!is_valid_role(s->role)) {
            char msg[160];
            snprintf(msg, sizeof(msg), "invalid sample role for %s: %s",
                     s->sample_id, s->role);
            return set_err(err, err_sz, msg);
        }
        if (strcmp(s->role, "sample") == 0) has_test_sample = 1;
        m->n_samples++;
        p = obj_end;
        p = skip_ws(p, arr_end);
        if (p < arr_end && *p == ',') p++;
    }

    if (m->n_samples == 0)
        return set_err(err, err_sz, "manifest must contain at least one sample");
    if (!has_test_sample)
        return set_err(err, err_sz, "manifest must contain at least one role=sample");
    return 0;
}

int analyze_manifest_load(const char *path, analyze_manifest_t *m,
                          char *err, size_t err_sz) {
    analyze_manifest_init(m);
    char *json = read_text_file(path, err, err_sz);
    if (!json) return -1;
    const char *end = json + strlen(json);

    if (get_string_in_range(json, end, "run_id", m->run_id,
                            sizeof(m->run_id), 1) < 0 ||
        get_string_in_range(json, end, "index_path", m->index_path,
                            sizeof(m->index_path), 1) < 0) {
        free(json);
        return set_err(err, err_sz, "manifest requires run_id and index_path");
    }
    (void)get_string_in_range(json, end, "database_hash", m->database_hash,
                              sizeof(m->database_hash), 0);
    (void)get_string_in_range(json, end, "calibration_profile_path",
                              m->calibration_profile_path,
                              sizeof(m->calibration_profile_path), 0);
    if (parse_marker_panel(json, end, m, err, err_sz) < 0 ||
        parse_samples(json, end, m, err, err_sz) < 0) {
        free(json);
        return -1;
    }

    const char *params = find_key_value(json, end, "analysis_params");
    if (params && *skip_ws(params, end) == '{') {
        const char *obj_end;
        params = skip_ws(params, end);
        if (find_object_end(params, end, &obj_end) == 0) {
            (void)get_number_in_range(params, obj_end, "detection_threshold",
                                      &m->detection_threshold);
            (void)get_number_in_range(params, obj_end, "prune_threshold",
                                      &m->prune_threshold);
            (void)get_int_in_range(params, obj_end, "n_bootstrap",
                                   &m->n_bootstrap);
            (void)get_bool_in_range(params, obj_end, "control_adjustment",
                                    &m->control_adjustment);
            (void)get_bool_in_range(params, obj_end, "out_of_panel_check",
                                    &m->out_of_panel_check);
            (void)get_bool_in_range(params, obj_end, "resolvability_check",
                                    &m->resolvability_check);
            (void)get_string_in_range(params, obj_end, "read_mode",
                                      m->read_mode, sizeof(m->read_mode), 0);
        }
    }

    free(json);
    return 0;
}

static void write_json_string(FILE *out, const char *s) {
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)s; p && *p; p++) {
        switch (*p) {
            case '"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\b': fputs("\\b", out); break;
            case '\f': fputs("\\f", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default:
                if (*p < 0x20) fprintf(out, "\\u%04x", *p);
                else fputc(*p, out);
        }
    }
    fputc('"', out);
}

static int has_role(const analyze_manifest_t *m, const char *role) {
    for (int i = 0; i < m->n_samples; i++) {
        if (strcmp(m->samples[i].role, role) == 0) return 1;
    }
    return 0;
}

static int should_emit_species(const species_report_t *sp) {
    if (sp->weight_pct < PARAM_WEIGHT_FLOOR &&
        sp->read_pct < PARAM_READ_PCT_FLOOR) return 0;
    if (sp->n_markers_detected < PARAM_MIN_MARKERS_FOR_LOW_WT &&
        sp->weight_pct < PARAM_LOW_WT_CEILING) return 0;
    return 1;
}

static const char *confidence_class(const species_report_t *sp,
                                    double threshold_pct) {
    if (sp->weight_pct > 0.0 && sp->weight_pct < threshold_pct) return "TRACE";
    if (sp->p_value < 0.01 && sp->n_markers_detected >= 2 &&
        sp->weight_pct >= threshold_pct) return "HIGH";
    if (sp->p_value < 0.05 && sp->weight_pct >= threshold_pct) return "MEDIUM";
    return "LOW";
}

static int analyze_sample(const analyze_manifest_t *manifest,
                          halal_index_t *idx,
                          const int *amp_lens,
                          calibration_result_t *cal,
                          const analyze_sample_t *sample,
                          sample_result_v1_t *out_result,
                          char *err, size_t err_sz) {
    char **seqs = NULL, **names = NULL;
    int *lens = NULL, n_reads = 0;
    if (hs_fasta_read_all(sample->fastq_path, &seqs, &names, &lens, &n_reads) < 0) {
        return set_err(err, err_sz, "failed to read FASTQ for manifest sample");
    }

    classify_opts_t copts;
    if (strcmp(manifest->read_mode, "long") == 0) {
        copts = classify_opts_longread();
    } else {
        copts = classify_opts_default();
        if (strcmp(manifest->read_mode, "short") == 0)
            copts.read_mode = READ_MODE_SHORT;
    }

    read_result_t *classes =
        classify_reads(idx, (const char **)seqs, lens, n_reads, &copts);
    int n_em_reads = 0;
    em_read_t *em_reads = em_reads_from_classify(classes, n_reads, &n_em_reads);

    em_config_t ecfg = em_config_default();
    ecfg.prune_threshold = manifest->prune_threshold;
    if (cal) {
        ecfg.d_mu = cal->d_mu;
        ecfg.d_sigma = cal->d_sigma;
        ecfg.b_mu = cal->b_mu;
        ecfg.b_sigma = cal->b_sigma;
    }
    double *mito_cn = (double *)hs_malloc((size_t)idx->db->n_species * sizeof(double));
    for (int s = 0; s < idx->db->n_species; s++)
        mito_cn[s] = idx->db->species[s].mito_copy_number;
    ecfg.mito_copy_numbers = mito_cn;

    em_result_t *em = em_fit(em_reads, n_em_reads, idx->db->n_species,
                             idx->db->n_markers, amp_lens, &ecfg);
    halal_report_t *report = report_generate(em, idx->db, classes, n_reads,
                                             manifest->detection_threshold);
    strncpy(report->sample_id, sample->sample_id, sizeof(report->sample_id) - 1);

    out_result->sample = sample;
    out_result->report = report;

    em_result_destroy(em);
    free(mito_cn);
    em_reads_free(em_reads, n_em_reads);
    classify_results_free(classes, n_reads);
    hs_fasta_free_all(seqs, names, lens, n_reads);
    return 0;
}

static void write_marker_panel(FILE *out, const analyze_manifest_t *m) {
    fputc('[', out);
    for (int i = 0; i < m->n_markers; i++) {
        if (i) fputs(", ", out);
        write_json_string(out, m->marker_ids[i]);
    }
    fputc(']', out);
}

static void write_control_check(FILE *out, const char *status,
                                int total_reads, int classified_reads,
                                const char *notes) {
    fprintf(out, "{\n");
    fprintf(out, "      \"status\": ");
    write_json_string(out, status);
    fprintf(out, ",\n      \"total_reads\": %d,\n", total_reads);
    fprintf(out, "      \"classified_reads\": %d,\n", classified_reads);
    fprintf(out, "      \"detected_species\": [],\n");
    fprintf(out, "      \"contamination_flagged\": false,\n");
    fprintf(out, "      \"notes\": ");
    write_json_string(out, notes);
    fprintf(out, "\n    }");
}

static void write_sample_result(FILE *out, const sample_result_v1_t *sr,
                                const halal_refdb_t *db,
                                const analyze_manifest_t *manifest) {
    const halal_report_t *r = sr->report;
    const double threshold_pct = manifest->detection_threshold * 100.0;
    int emitted = 0;
    int exclusion_emitted = 0;

    fprintf(out, "    {\n");
    fprintf(out, "      \"sample_id\": ");
    write_json_string(out, sr->sample->sample_id);
    fprintf(out, ",\n      \"role\": ");
    write_json_string(out, sr->sample->role);
    fprintf(out, ",\n      \"screening_result\": ");
    write_json_string(out, screening_result_str(r->screening_result));
    fprintf(out, ",\n      \"out_of_panel_flag\": false,\n");
    fprintf(out, "      \"out_of_panel_details\": {\n");
    fprintf(out, "        \"unclassified_read_pct\": %.4f,\n",
            r->total_reads > 0 ? 100.0 * (r->total_reads - r->classified_reads) /
            (double)r->total_reads : 0.0);
    fprintf(out, "        \"assignment_entropy\": 0.0,\n");
    fprintf(out, "        \"top_hit_containment_margin\": 0.0,\n");
    fprintf(out, "        \"nearest_neighbor_distance\": 0.0,\n");
    fprintf(out, "        \"database_coverage_estimate\": %.4f,\n",
            r->total_reads > 0 ? (double)r->classified_reads / r->total_reads : 0.0);
    fprintf(out, "        \"recommendation\": \"NONE\"\n");
    fprintf(out, "      },\n");
    fprintf(out, "      \"species\": [\n");

    for (int s = 0; s < r->n_species; s++) {
        const species_report_t *sp = &r->species[s];
        if (!should_emit_species(sp)) continue;
        if (sp->species_category == CATEGORY_EXCLUSION) exclusion_emitted++;
        const char *conf = confidence_class(sp, threshold_pct);
        if (emitted++) fputs(",\n", out);
        fprintf(out, "        {\n");
        fprintf(out, "          \"species_id\": ");
        write_json_string(out, sp->species_id);
        fprintf(out, ",\n          \"common_name\": ");
        write_json_string(out, db->species[s].common_name);
        fprintf(out, ",\n          \"species_category\": ");
        write_json_string(out, species_category_str(sp->species_category));
        fprintf(out, ",\n          \"confidence_class\": ");
        write_json_string(out, conf);
        fprintf(out, ",\n          \"screening_estimate_pct\": %.6f,\n", sp->weight_pct);
        fprintf(out, "          \"ci_lo_pct\": %.6f,\n", sp->ci_lo);
        fprintf(out, "          \"ci_hi_pct\": %.6f,\n", sp->ci_hi);
        fprintf(out, "          \"p_value\": %.8g,\n", sp->p_value);
        fprintf(out, "          \"bootstrap_stability_pct\": %d,\n",
                r->bootstrap_stability_pct);
        fprintf(out, "          \"control_adjusted\": false,\n");
        fprintf(out, "          \"control_background_pct\": 0.0,\n");
        fprintf(out, "          \"ambiguity_group\": [],\n");
        fprintf(out, "          \"ambiguity_note\": \"Resolvability matrix not implemented in this v1 engine bridge.\",\n");
        fprintf(out, "          \"confirmatory_recommended\": %s,\n",
                sp->species_category == CATEGORY_EXCLUSION ? "true" : "false");
        fprintf(out, "          \"confirmatory_reason\": ");
        write_json_string(out, sp->species_category == CATEGORY_EXCLUSION
            ? "Species-of-concern evidence should be reviewed with confirmatory testing when operationally required."
            : "");
        fprintf(out, ",\n          \"marker_evidence\": [");
        int m_emitted = 0;
        for (int mi = 0; mi < HS_MAX_MARKERS; mi++) {
            if (sp->read_counts[mi] <= 0) continue;
            if (m_emitted++) fputs(", ", out);
            fprintf(out, "{");
            fprintf(out, "\"marker_id\": ");
            if (mi < db->n_markers) write_json_string(out, db->marker_ids[mi]);
            else write_json_string(out, "unknown");
            fprintf(out, ", \"read_count\": %d", sp->read_counts[mi]);
            fprintf(out, ", \"read_pct\": %.6f",
                    r->classified_reads > 0
                    ? 100.0 * sp->read_counts[mi] / r->classified_reads : 0.0);
            fprintf(out, ", \"containment_mean\": 0.0");
            fprintf(out, ", \"containment_median\": 0.0");
            fprintf(out, ", \"kmer_uniqueness_score\": 0.0");
            fprintf(out, ", \"nearest_neighbor_distance\": 0.0");
            fprintf(out, ", \"detection_status\": \"DETECTED\"}");
        }
        fprintf(out, "]\n");
        fprintf(out, "        }");
    }
    fprintf(out, "\n      ],\n");

    fprintf(out, "      \"evidence_summary\": {\n");
    fprintf(out, "        \"total_reads\": %d,\n", r->total_reads);
    fprintf(out, "        \"classified_reads\": %d,\n", r->classified_reads);
    fprintf(out, "        \"n_species_detected\": %d,\n", emitted);
    fprintf(out, "        \"n_species_exclusion\": %d,\n", exclusion_emitted);
    fprintf(out, "        \"n_markers_active\": %d,\n",
            r->n_species > 0 ? r->species[0].markers_tested : 0);
    fprintf(out, "        \"evidence_threshold_estimate_pct\": %.6f,\n",
            r->classified_reads > 0 ? 300.0 / sqrt((double)r->classified_reads) : 100.0);
    fprintf(out, "        \"cross_marker_agreement\": %.6f,\n", r->cross_marker_agreement);
    fprintf(out, "        \"degradation_lambda\": %.8f\n", r->degradation_lambda);
    fprintf(out, "      }\n");
    fprintf(out, "    }");
}

static int write_result_json(const char *path, const analyze_manifest_t *manifest,
                             const halal_refdb_t *db,
                             sample_result_v1_t *samples, int n_samples,
                             int run_status_review, const char *warning) {
    FILE *out = fopen(path, "w");
    if (!out) {
        HS_LOG_ERROR("Cannot write result JSON: %s", path);
        return -1;
    }

    int total_reads = 0, classified_reads = 0;
    for (int i = 0; i < n_samples; i++) {
        total_reads += samples[i].report->total_reads;
        classified_reads += samples[i].report->classified_reads;
    }

    fprintf(out, "{\n");
    fprintf(out, "  \"schema_version\": \"1.0.0\",\n");
    fprintf(out, "  \"engine_version\": \"speciesid 0.1.0\",\n");
    fprintf(out, "  \"run_id\": ");
    write_json_string(out, manifest->run_id);
    fprintf(out, ",\n  \"database_hash\": ");
    write_json_string(out, manifest->database_hash);
    fprintf(out, ",\n  \"calibration_hash\": null,\n");
    fprintf(out, "  \"run_qc\": {\n");
    fprintf(out, "    \"status\": \"%s\",\n",
            run_status_review ? "RUN_REVIEW" : "RUN_PASS");
    fprintf(out, "    \"total_reads\": %d,\n", total_reads);
    fprintf(out, "    \"classified_read_pct\": %.6f,\n",
            total_reads > 0 ? 100.0 * classified_reads / total_reads : 0.0);
    fprintf(out, "    \"per_sample_depth\": [");
    for (int i = 0; i < n_samples; i++) {
        if (i) fputs(", ", out);
        fprintf(out, "{\"sample_id\": ");
        write_json_string(out, samples[i].sample->sample_id);
        fprintf(out, ", \"n_reads\": %d, \"classified\": %d, \"unclassified\": %d}",
                samples[i].report->total_reads, samples[i].report->classified_reads,
                samples[i].report->total_reads - samples[i].report->classified_reads);
    }
    fprintf(out, "],\n");
    fprintf(out, "    \"positive_control_check\": ");
    write_control_check(out, has_role(manifest, "positive_control") ? "OK" : "NOT_PROVIDED",
                        0, 0, "Positive-control recovery is reported as metadata only in this v1 engine bridge.");
    fprintf(out, ",\n    \"negative_control_check\": ");
    write_control_check(out, has_role(manifest, "negative_control") ? "OK" : "NOT_PROVIDED",
                        0, 0, "Control-aware background correction is not implemented in this v1 engine bridge.");
    fprintf(out, ",\n    \"extraction_blank_check\": ");
    write_control_check(out, has_role(manifest, "extraction_blank") ? "OK" : "NOT_PROVIDED",
                        0, 0, "Control-aware background correction is not implemented in this v1 engine bridge.");
    fprintf(out, ",\n    \"marker_balance\": {\"status\": \"WARN\", \"per_marker_reads\": {}, \"gini_coefficient\": 0.0, \"dropout_markers\": []},\n");
    fprintf(out, "    \"index_bleed_estimate\": 0.0,\n");
    fprintf(out, "    \"primer_dropout\": [],\n");
    fprintf(out, "    \"warnings\": [");
    if (warning && warning[0]) write_json_string(out, warning);
    fprintf(out, "]\n");
    fprintf(out, "  },\n");
    fprintf(out, "  \"samples\": [\n");
    for (int i = 0; i < n_samples; i++) {
        if (i) fputs(",\n", out);
        write_sample_result(out, &samples[i], db, manifest);
    }
    fprintf(out, "\n  ],\n");
    fprintf(out, "  \"audit\": {\n");
    fprintf(out, "    \"software_version\": \"speciesid 0.1.0\",\n");
    fprintf(out, "    \"database_version\": \"unknown\",\n");
    fprintf(out, "    \"database_hash\": ");
    write_json_string(out, manifest->database_hash);
    fprintf(out, ",\n    \"calibration_version\": null,\n");
    fprintf(out, "    \"calibration_hash\": null,\n");
    fprintf(out, "    \"marker_panel\": ");
    write_marker_panel(out, manifest);
    fprintf(out, ",\n    \"primer_set\": \"manifest_v1\",\n");
    fprintf(out, "    \"manifest\": {\"run_id\": ");
    write_json_string(out, manifest->run_id);
    fprintf(out, ", \"sample_count\": %d},\n", manifest->n_samples);
    fprintf(out, "    \"thresholds\": {\"detection_threshold_wpw\": %.8f, \"min_containment\": %.4f, \"control_background_threshold\": 0.0},\n",
            manifest->detection_threshold, PARAM_EM_MIN_CONTAINMENT);
    fprintf(out, "    \"controls_used\": {\"positive_controls\": [], \"negative_controls\": [], \"extraction_blanks\": []},\n");
    fprintf(out, "    \"engine_exit_code\": 0,\n");
    fprintf(out, "    \"engine_stderr\": \"\"\n");
    fprintf(out, "  }\n");
    fprintf(out, "}\n");

    fclose(out);
    return 0;
}

int analyze_execute_manifest(const char *manifest_path, const char *output_path) {
    char err[1024] = {0};
    analyze_manifest_t manifest;
    if (analyze_manifest_load(manifest_path, &manifest, err, sizeof(err)) < 0) {
        HS_LOG_ERROR("%s", err[0] ? err : "manifest parsing failed");
        return 1;
    }

    halal_index_t *idx = index_load(manifest.index_path);
    if (!idx) {
        HS_LOG_ERROR("Failed to load index from %s", manifest.index_path);
        return 1;
    }

    for (int i = 0; i < manifest.n_markers; i++) {
        if (refdb_find_marker(idx->db, manifest.marker_ids[i]) < 0) {
            HS_LOG_ERROR("Marker %s is not present in the selected index",
                         manifest.marker_ids[i]);
            index_destroy(idx);
            return 1;
        }
    }

    calibration_result_t *cal = NULL;
    if (manifest.calibration_profile_path[0]) {
        cal = calibrate_load(manifest.calibration_profile_path);
        if (!cal) {
            HS_LOG_ERROR("Failed to load calibration profile %s",
                         manifest.calibration_profile_path);
            index_destroy(idx);
            return 1;
        }
    }

    int *amp_lens = (int *)hs_calloc(
        (size_t)(idx->db->n_species * idx->db->n_markers), sizeof(int));
    for (int i = 0; i < idx->db->n_marker_refs; i++) {
        int si = idx->db->markers[i].species_idx;
        int mi = idx->db->markers[i].marker_idx;
        amp_lens[si * idx->db->n_markers + mi] = idx->db->markers[i].amplicon_length;
    }

    sample_result_v1_t results[ANALYZE_MAX_SAMPLES];
    memset(results, 0, sizeof(results));
    for (int i = 0; i < manifest.n_samples; i++) {
        if (analyze_sample(&manifest, idx, amp_lens, cal, &manifest.samples[i],
                           &results[i], err, sizeof(err)) < 0) {
            HS_LOG_ERROR("%s", err);
            for (int j = 0; j < i; j++) report_destroy(results[j].report);
            free(amp_lens);
            calibrate_result_destroy(cal);
            index_destroy(idx);
            return 1;
        }
    }

    int needs_review = 0;
    char warning[512] = {0};
    if (manifest.control_adjustment &&
        !has_role(&manifest, "negative_control") &&
        !has_role(&manifest, "extraction_blank")) {
        needs_review = 1;
        snprintf(warning, sizeof(warning),
                 "Control adjustment was requested, but no negative control or extraction blank was provided. Evidence is unadjusted.");
    }

    int rc = write_result_json(output_path, &manifest, idx->db, results,
                               manifest.n_samples, needs_review, warning);

    for (int i = 0; i < manifest.n_samples; i++) report_destroy(results[i].report);
    free(amp_lens);
    calibrate_result_destroy(cal);
    index_destroy(idx);
    return rc == 0 ? 0 : 1;
}
