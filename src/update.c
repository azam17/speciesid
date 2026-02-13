/*
 * update.c — Database auto-update implementation for SpeciesID.
 *
 * Uses system curl for HTTP, shasum / certutil for SHA-256.
 * No new library dependencies.
 */

#include "update.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
  #include <direct.h>
  #include <io.h>
  #define SID_MKDIR(p) _mkdir(p)
  #define SID_PATH_SEP '\\'
  #define SID_TMP_DIR_ENV "TEMP"
  #define SID_TMP_FALLBACK "C:\\Temp"
#else
  #include <unistd.h>
  #define SID_MKDIR(p) mkdir(p, 0755)
  #define SID_PATH_SEP '/'
  #define SID_TMP_DIR_ENV "TMPDIR"
  #define SID_TMP_FALLBACK "/tmp"
#endif

/* ================================================================== */
/* sid_update_init                                                     */
/* ================================================================== */
void sid_update_init(sid_update_ctx_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->status = SID_UPDATE_NONE;
}

/* ================================================================== */
/* sid_config_dir                                                      */
/* ================================================================== */
int sid_config_dir(char *out, size_t sz) {
#ifdef _WIN32
    const char *base = getenv("APPDATA");
    if (!base) base = getenv("USERPROFILE");
    if (!base) return 0;
    snprintf(out, sz, "%s\\SpeciesID", base);
#else
    const char *base = getenv("HOME");
    if (!base) return 0;
    snprintf(out, sz, "%s/.speciesid", base);
#endif
    return 1;
}

/* ================================================================== */
/* sid_update_read_local_version                                       */
/* ================================================================== */
uint32_t sid_update_read_local_version(const char *config_dir) {
    char path[1024];
    snprintf(path, sizeof(path), "%s%cdb_version", config_dir, SID_PATH_SEP);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    uint32_t ver = 0;
    if (fscanf(f, "%u", &ver) != 1) ver = 0;
    fclose(f);
    return ver;
}

/* ================================================================== */
/* sid_update_write_local_version                                      */
/* ================================================================== */
int sid_update_write_local_version(const char *config_dir, uint32_t ver) {
    SID_MKDIR(config_dir);
    char path[1024];
    snprintf(path, sizeof(path), "%s%cdb_version", config_dir, SID_PATH_SEP);
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "%u\n", ver);
    fclose(f);
    return 1;
}

/* ================================================================== */
/* sid_manifest_parse                                                  */
/* ================================================================== */
/*
 * Expected format (fixed keys, no nesting):
 *   {"version":3,"url":"https://...","sha256":"abc...","size":12345}
 *
 * Uses strstr + simple extraction — no JSON library required.
 */
int sid_manifest_parse(const char *json, sid_manifest_t *out) {
    memset(out, 0, sizeof(*out));

    /* version (integer) */
    const char *p = strstr(json, "\"version\"");
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    out->version = (uint32_t)strtoul(p, NULL, 10);
    if (out->version == 0) return 0;

    /* url (string) */
    p = strstr(json, "\"url\"");
    if (!p) return 0;
    p = strchr(p + 5, '"');   /* opening quote of key's colon value */
    if (!p) return 0;
    /* skip past the colon, find the opening quote of the value */
    p = strchr(p + 1, '"');
    if (!p) return 0;
    p++;  /* past opening quote */
    {
        const char *end = strchr(p, '"');
        if (!end) return 0;
        size_t len = (size_t)(end - p);
        if (len >= sizeof(out->url)) len = sizeof(out->url) - 1;
        memcpy(out->url, p, len);
        out->url[len] = '\0';
    }

    /* sha256 (string, 64 hex chars) */
    p = strstr(json, "\"sha256\"");
    if (!p) return 0;
    p = strchr(p + 8, '"');
    if (!p) return 0;
    p = strchr(p + 1, '"');
    if (!p) return 0;
    p++;
    {
        const char *end = strchr(p, '"');
        if (!end) return 0;
        size_t len = (size_t)(end - p);
        if (len >= sizeof(out->sha256)) len = sizeof(out->sha256) - 1;
        memcpy(out->sha256, p, len);
        out->sha256[len] = '\0';
    }

    /* size (integer) */
    p = strstr(json, "\"size\"");
    if (!p) return 0;
    p = strchr(p + 6, ':');
    if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    out->size_bytes = (uint64_t)strtoull(p, NULL, 10);

    return 1;
}

/* ================================================================== */
/* sid_update_fetch_manifest                                           */
/* ================================================================== */
int sid_update_fetch_manifest(sid_update_ctx_t *ctx) {
    char cmd[1200];
    snprintf(cmd, sizeof(cmd),
             "curl -sS --connect-timeout 10 --max-time 30 \"%s\" 2>&1",
             SID_UPDATE_URL);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Failed to run curl");
        return 0;
    }

    char buf[4096];
    size_t total = 0;
    while (total < sizeof(buf) - 1) {
        size_t n = fread(buf + total, 1, sizeof(buf) - 1 - total, fp);
        if (n == 0) break;
        total += n;
    }
    buf[total] = '\0';
    int ret = pclose(fp);

    if (ret != 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "curl failed: %.200s", buf);
        return 0;
    }

    if (!sid_manifest_parse(buf, &ctx->manifest)) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Failed to parse manifest JSON");
        return 0;
    }

    return 1;
}

/* ================================================================== */
/* sid_update_download                                                 */
/* ================================================================== */
int sid_update_download(sid_update_ctx_t *ctx,
                       char *tmp_path_out, size_t tmp_sz) {
    const char *tmp_dir = getenv(SID_TMP_DIR_ENV);
    if (!tmp_dir || !tmp_dir[0]) tmp_dir = SID_TMP_FALLBACK;

    snprintf(tmp_path_out, tmp_sz, "%s%c_sid_update.idx.tmp",
             tmp_dir, SID_PATH_SEP);

    char cmd[2200];
    snprintf(cmd, sizeof(cmd),
             "curl -sS --connect-timeout 15 --max-time 600 "
             "-o \"%s\" \"%s\" 2>&1",
             tmp_path_out, ctx->manifest.url);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Failed to run curl for download");
        return 0;
    }

    /* Read any error output */
    char err_buf[1024];
    size_t total = 0;
    while (total < sizeof(err_buf) - 1) {
        size_t n = fread(err_buf + total, 1, sizeof(err_buf) - 1 - total, fp);
        if (n == 0) break;
        total += n;
    }
    err_buf[total] = '\0';
    int ret = pclose(fp);

    if (ret != 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Download failed: %.200s", err_buf);
        return 0;
    }

    /* Verify file exists and has non-zero size */
    struct stat st;
    if (stat(tmp_path_out, &st) != 0 || st.st_size == 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Downloaded file is missing or empty");
        return 0;
    }

    return 1;
}

/* ================================================================== */
/* sid_update_verify_sha256                                            */
/* ================================================================== */
int sid_update_verify_sha256(const char *file_path,
                            const char *expected_hex,
                            char *error_msg, size_t err_sz) {
    char cmd[1200];
#ifdef _WIN32
    /* certutil -hashfile <file> SHA256 outputs the hash on line 2 */
    snprintf(cmd, sizeof(cmd),
             "certutil -hashfile \"%s\" SHA256 2>&1", file_path);
#else
    /* shasum -a 256 outputs: <hash>  <filename> */
    snprintf(cmd, sizeof(cmd),
             "shasum -a 256 \"%s\" 2>&1", file_path);
#endif

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        snprintf(error_msg, err_sz, "Failed to run hash command");
        return 0;
    }

    char output[2048];
    size_t total = 0;
    while (total < sizeof(output) - 1) {
        size_t n = fread(output + total, 1, sizeof(output) - 1 - total, fp);
        if (n == 0) break;
        total += n;
    }
    output[total] = '\0';
    int ret = pclose(fp);

    if (ret != 0) {
        snprintf(error_msg, err_sz,
                 "Hash command failed: %.200s", output);
        return 0;
    }

    /* Extract hex digest from output */
    char actual_hex[65] = {0};

#ifdef _WIN32
    /* certutil output:
     * SHA256 hash of <file>:
     * <hex string with possible spaces>
     * CertUtil: -hashfile command completed successfully.
     *
     * The hash is on the second line. */
    {
        const char *line2 = strchr(output, '\n');
        if (line2) {
            line2++;
            /* Skip whitespace */
            while (*line2 == ' ' || *line2 == '\r' || *line2 == '\t')
                line2++;
            /* Copy hex chars, skipping spaces */
            int j = 0;
            for (int i = 0; line2[i] && j < 64; i++) {
                if (isxdigit((unsigned char)line2[i]))
                    actual_hex[j++] = (char)tolower((unsigned char)line2[i]);
                else if (line2[i] == '\n' || line2[i] == '\r')
                    break;
            }
            actual_hex[j] = '\0';
        }
    }
#else
    /* shasum output: "<64 hex>  filename\n" */
    {
        int j = 0;
        for (int i = 0; output[i] && j < 64; i++) {
            if (isxdigit((unsigned char)output[i]))
                actual_hex[j++] = (char)tolower((unsigned char)output[i]);
            else
                break;
        }
        actual_hex[j] = '\0';
    }
#endif

    /* Compare (case-insensitive) */
    if (strlen(actual_hex) != 64) {
        snprintf(error_msg, err_sz,
                 "Failed to extract SHA-256 (got %zu chars)", strlen(actual_hex));
        return 0;
    }

    /* Lowercase the expected for comparison */
    char expected_lower[65];
    for (int i = 0; i < 64 && expected_hex[i]; i++)
        expected_lower[i] = (char)tolower((unsigned char)expected_hex[i]);
    expected_lower[64] = '\0';

    if (strncmp(actual_hex, expected_lower, 64) != 0) {
        snprintf(error_msg, err_sz,
                 "SHA-256 mismatch: expected %.16s..., got %.16s...",
                 expected_lower, actual_hex);
        return 0;
    }

    return 1;
}

/* ================================================================== */
/* Copy file (fallback for cross-filesystem rename)                    */
/* ================================================================== */
static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return 0;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return 0; }

    char buf[65536];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    }
    if (ferror(in)) ok = 0;

    fclose(out);
    fclose(in);
    return ok;
}

/* ================================================================== */
/* sid_update_install                                                  */
/* ================================================================== */
int sid_update_install(const char *tmp_path, const char *dest_path,
                      char *error_msg, size_t err_sz) {
    /* Try atomic rename first (works on same filesystem) */
    if (rename(tmp_path, dest_path) == 0)
        return 1;

    /* Cross-filesystem fallback: backup old, copy new, remove backup */
    if (errno != EXDEV) {
        snprintf(error_msg, err_sz,
                 "rename() failed: %s", strerror(errno));
        return 0;
    }

    /* Backup existing file */
    char backup[1088];
    snprintf(backup, sizeof(backup), "%s.bak", dest_path);
    /* Remove any stale backup */
    remove(backup);

    /* Back up current index (may not exist on first install) */
    int had_existing = 0;
    if (rename(dest_path, backup) == 0) {
        had_existing = 1;
    }

    /* Copy new file into place */
    if (!copy_file(tmp_path, dest_path)) {
        /* Restore backup */
        if (had_existing) rename(backup, dest_path);
        snprintf(error_msg, err_sz,
                 "Failed to copy new index file");
        return 0;
    }

    /* Clean up */
    if (had_existing) remove(backup);
    remove(tmp_path);
    return 1;
}

/* ================================================================== */
/* sid_update_run — full orchestrator                                  */
/* ================================================================== */
void sid_update_run(sid_update_ctx_t *ctx) {
    /* Resolve config dir */
    if (!ctx->config_dir[0]) {
        if (!sid_config_dir(ctx->config_dir, sizeof(ctx->config_dir))) {
            snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                     "Cannot determine config directory");
            ctx->status = SID_UPDATE_ERROR;
            return;
        }
    }

    /* Read local version */
    ctx->local_version = sid_update_read_local_version(ctx->config_dir);

    /* Fetch manifest */
    ctx->status = SID_UPDATE_CHECKING;
    ctx->progress_pct = 0;
    if (!sid_update_fetch_manifest(ctx)) {
        ctx->status = SID_UPDATE_ERROR;
        return;
    }

    /* Compare versions */
    if (ctx->manifest.version <= ctx->local_version) {
        ctx->status = SID_UPDATE_NONE;  /* already up to date */
        return;
    }

    ctx->status = SID_UPDATE_AVAILABLE;
    /* Note: the GUI can read status here and show the notification.
     * If this is called as a check-only operation, the caller should
     * stop after seeing AVAILABLE.  For a full update, continue: */
}

/* Separate download+install step (called after user confirms) */
void sid_update_run_download(sid_update_ctx_t *ctx) {
    /* Download */
    ctx->status = SID_UPDATE_DOWNLOADING;
    ctx->progress_pct = 10;
    char tmp_path[1024];
    if (!sid_update_download(ctx, tmp_path, sizeof(tmp_path))) {
        ctx->status = SID_UPDATE_ERROR;
        return;
    }
    ctx->progress_pct = 70;

    /* Verify SHA-256 */
    ctx->status = SID_UPDATE_VERIFYING;
    ctx->progress_pct = 75;
    if (!sid_update_verify_sha256(tmp_path, ctx->manifest.sha256,
                                  ctx->error_msg, sizeof(ctx->error_msg))) {
        remove(tmp_path);
        ctx->status = SID_UPDATE_ERROR;
        return;
    }
    ctx->progress_pct = 85;

    /* Install */
    ctx->status = SID_UPDATE_INSTALLING;
    ctx->progress_pct = 90;
    if (!sid_update_install(tmp_path, ctx->index_path,
                           ctx->error_msg, sizeof(ctx->error_msg))) {
        ctx->status = SID_UPDATE_ERROR;
        return;
    }

    /* Write new version */
    sid_update_write_local_version(ctx->config_dir, ctx->manifest.version);
    ctx->local_version = ctx->manifest.version;

    ctx->progress_pct = 100;
    ctx->status = SID_UPDATE_DONE;
}
