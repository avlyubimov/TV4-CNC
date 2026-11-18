#pragma once
#include "https.h"
#include "envelope.h"
#include "image_stream.h"
/* Read-only, single worker-owned operation. No task, UI, storage or install.
 * Zero initialize once; reset after every terminal result before reuse.
 * Request/context/strings must remain immutable outside the session workspace.
 * No concurrent calls/reads: future UI must receive copied snapshots. */
typedef enum { TV4_SESSION_IDLE,TV4_SESSION_MANIFEST,TV4_SESSION_ENVELOPE,
    TV4_SESSION_AUTHENTICATING,TV4_SESSION_IMAGE,TV4_SESSION_FINISHING,
    TV4_SESSION_CANDIDATE_READY,TV4_SESSION_IMAGE_VERIFIED,
    TV4_SESSION_FAILED,TV4_SESSION_CANCELLED } tv4_session_state;
typedef enum { TV4_SESSION_NONE,TV4_SESSION_ARGUMENT,TV4_SESSION_TRANSPORT,
    TV4_SESSION_POLICY,TV4_SESSION_FORMAT,TV4_SESSION_TIMEOUT,
    TV4_SESSION_CANCEL } tv4_session_error;
typedef struct {
    tv4_session_state state,failed_stage;
    tv4_session_error error;
    tv4_http_result http;
    tv4_ota_result policy;
    tv4_image_error image;
    uint32_t image_bytes;
} tv4_session_result;
typedef struct {
    /* Exact manifest.json URL; manifest.sig is derived from the same release. */
    const char *manifest_url;
    const tv4_ota_context *trust;
    tv4_image_limits image_limits;
    uint32_t idle_timeout_ms,total_timeout_ms,io_timeout_ms;
    unsigned maximum_redirects;
    bool trusted_time,verify_image;
} tv4_session_request;
typedef struct {
    void *context;
    /* Trusted sequential transport, must honor the bounded HTTP contract. */
    tv4_http_result (*fetch)(void *,const tv4_http_request *,tv4_http_workspace *,
        tv4_http_consume,void *,tv4_http_report *);
    uint32_t (*now_ms)(void *);
    bool (*cancelled)(void *);
} tv4_session_io;
typedef struct {
    tv4_session_result result;
    tv4_ota_candidate candidate; /* Cleared on failure/cancel; not install approval. */
    tv4_http_workspace http;
    tv4_image_stream image;
    uint8_t manifest[TV4_OTA_MANIFEST_MAX],envelope[TV4_OTA_ENVELOPE_MAX];
    size_t manifest_length,envelope_length;
    char envelope_url[TV4_HTTP_URL_MAX+1];
} tv4_ota_session;
/* Returns true only for CANDIDATE_READY / IMAGE_VERIFIED. All downloaded data
 * remains read-only; IMAGE_VERIFIED is integrity/format, not bootability. */
bool tv4_ota_session_run(tv4_ota_session *,const tv4_session_request *,const tv4_session_io *);
/* Single owner, outside run only. Releases hash contexts and clears all state. */
void tv4_ota_session_reset(tv4_ota_session *);
