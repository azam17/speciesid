/*
 * update.h — Database auto-update system for SpeciesID.
 *
 * Fetches a JSON manifest from a remote server, compares versions,
 * downloads the new index file, verifies its SHA-256 digest, and
 * atomically installs it.  No new dependencies: uses curl (built
 * into macOS and Windows 10+) and shasum / certutil for hashing.
 */

#ifndef SPECIESID_UPDATE_H
#define SPECIESID_UPDATE_H

#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Compile-time configurable server URL                                */
/* Override with -DSID_UPDATE_URL=\"https://your.server/manifest.json\" */
/* ------------------------------------------------------------------ */
#ifndef SID_UPDATE_URL
#define SID_UPDATE_URL "https://speciesid.org/db/manifest.json"
#endif

/* ------------------------------------------------------------------ */
/* Status enum                                                         */
/* ------------------------------------------------------------------ */
typedef enum {
    SID_UPDATE_NONE        = 0,  /* no update activity                */
    SID_UPDATE_CHECKING    = 1,  /* fetching manifest                 */
    SID_UPDATE_AVAILABLE   = 2,  /* newer version exists              */
    SID_UPDATE_DOWNLOADING = 3,  /* downloading new index             */
    SID_UPDATE_VERIFYING   = 4,  /* checking SHA-256                  */
    SID_UPDATE_INSTALLING  = 5,  /* atomic rename into place          */
    SID_UPDATE_DONE        = 6,  /* successfully installed            */
    SID_UPDATE_ERROR       = 7   /* something went wrong              */
} sid_update_status_t;

/* ------------------------------------------------------------------ */
/* Manifest (parsed from server JSON)                                  */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t version;           /* monotonically increasing          */
    char     url[1024];         /* download URL for the .idx file    */
    char     sha256[65];        /* hex-encoded SHA-256 digest        */
    uint64_t size_bytes;        /* expected file size                */
} sid_manifest_t;

/* ------------------------------------------------------------------ */
/* Update context (shared between GUI thread and worker thread)        */
/* ------------------------------------------------------------------ */
typedef struct {
    volatile int        status;         /* sid_update_status_t        */
    volatile int        progress_pct;   /* 0-100 download progress   */
    char                error_msg[256]; /* human-readable error      */
    sid_manifest_t       manifest;       /* parsed remote manifest    */
    uint32_t            local_version;  /* currently installed ver    */
    char                index_path[1024]; /* path to installed .idx  */
    char                config_dir[1024]; /* ~/.speciesid or %APPDATA%*/
} sid_update_ctx_t;

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/* Zero-initialise the update context. */
void sid_update_init(sid_update_ctx_t *ctx);

/* Resolve the platform config directory.
 * Unix:    $HOME/.speciesid/
 * Windows: %APPDATA%\SpeciesID\
 * Returns 1 on success, 0 on failure (no HOME / APPDATA). */
int sid_config_dir(char *out, size_t sz);

/* Read db_version from <config_dir>/db_version.
 * Returns the version number, or 0 if not found. */
uint32_t sid_update_read_local_version(const char *config_dir);

/* Write a new db_version file. Returns 1 on success. */
int sid_update_write_local_version(const char *config_dir, uint32_t ver);

/* Fetch the remote manifest JSON via curl.
 * Populates ctx->manifest on success.
 * Returns 1 on success, 0 on failure (sets ctx->error_msg). */
int sid_update_fetch_manifest(sid_update_ctx_t *ctx);

/* Parse a fixed-format manifest JSON string into *out.
 * Returns 1 on success, 0 on parse error. */
int sid_manifest_parse(const char *json, sid_manifest_t *out);

/* Download the index file to a temporary path.
 * Returns 1 on success.  tmp_path_out receives the path. */
int sid_update_download(sid_update_ctx_t *ctx,
                       char *tmp_path_out, size_t tmp_sz);

/* Verify SHA-256 of a file.  Expected digest in hex (lowercase).
 * Uses shasum on macOS/Linux, certutil on Windows.
 * Returns 1 if digest matches. */
int sid_update_verify_sha256(const char *file_path,
                            const char *expected_hex,
                            char *error_msg, size_t err_sz);

/* Atomically install tmp_path -> dest_path.
 * Tries rename(); falls back to copy+rename for cross-filesystem.
 * Returns 1 on success. */
int sid_update_install(const char *tmp_path, const char *dest_path,
                      char *error_msg, size_t err_sz);

/* Full orchestrator: fetch manifest -> compare version.
 * Sets status to SID_UPDATE_AVAILABLE if newer version exists,
 * or SID_UPDATE_NONE if up to date.  Call from a background thread. */
void sid_update_run(sid_update_ctx_t *ctx);

/* Download, verify, and install the update.
 * Call after sid_update_run() sets status to SID_UPDATE_AVAILABLE.
 * Updates ctx->status throughout.  Call from a background thread. */
void sid_update_run_download(sid_update_ctx_t *ctx);

#endif /* SPECIESID_UPDATE_H */
