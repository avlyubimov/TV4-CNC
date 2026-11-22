#pragma once
#include "https.h"
#include "envelope.h"
#include "image_stream.h"
#include "resource_guard.h"
/* Single worker-owned operation. Existing run APIs are read-only; only the
 * explicit install_guarded entry accepts a storage sink. No owned task or UI.
 * Zero initialize once; reset after every terminal result before reuse.
 * Request/context/strings must remain immutable outside the session workspace.
 * No concurrent calls/reads: future UI must receive copied snapshots. */
typedef enum { TV4_SESSION_IDLE,TV4_SESSION_MANIFEST,TV4_SESSION_ENVELOPE,
    TV4_SESSION_AUTHENTICATING,TV4_SESSION_IMAGE,TV4_SESSION_FINISHING,
    TV4_SESSION_CANDIDATE_READY,TV4_SESSION_IMAGE_VERIFIED,
    TV4_SESSION_FAILED,TV4_SESSION_CANCELLED,TV4_SESSION_INSTALLED } tv4_session_state;
typedef enum { TV4_SESSION_NONE,TV4_SESSION_ARGUMENT,TV4_SESSION_TRANSPORT,
    TV4_SESSION_POLICY,TV4_SESSION_FORMAT,TV4_SESSION_TIMEOUT,
    TV4_SESSION_CANCEL,TV4_SESSION_RESOURCE,TV4_SESSION_STORAGE } tv4_session_error;
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
 * remains read-only; IMAGE_VERIFIED is integrity/format, not bootability.
 * Legacy portable coordinator: no resource admission. Not a runtime enable gate. */
bool tv4_ota_session_run(tv4_ota_session *,const tv4_session_request *,const tv4_session_io *);
/* Resource-checked read-only entry, also used by the runtime metadata check.
 * Policy is immutable; reader/context outlive run. Report is writable by this
 * runner only, disjoint from workspace/inputs, same worker. No default floors.
 * Checks before/after fetch/crypto/finish and in consumers, not inside TLS/SDK.
 * Resource failure is sticky through cleanup; candidate/metadata/HTTP cleared,
 * hash contexts closed.
 * Report is separate so the bounded session workspace does not grow. */
bool tv4_ota_session_run_guarded(tv4_ota_session *,const tv4_session_request *,const tv4_session_io *,
    const tv4_ota_resource_policy *,tv4_ota_resource_reader,void *,tv4_ota_resource_report *);
/* Explicit installer path; existing run APIs remain read-only. Single worker.
 * begin is called only after signature/policy and must independently bind the
 * candidate to the operator's approved version/hash and an inactive partition.
 * Every chunk passes the existing verifier before write. seal performs SDK
 * validation; activate is LAST, after full digest/EOF/cleanup/resource/cancel
 * checks. No failing check may follow activation. abort must be idempotent,
 * including a partially failed begin/seal. No pointer retained after run. */
typedef struct {
    void *context;
    bool (*begin)(void *,const tv4_ota_candidate *);
    bool (*write)(void *,const uint8_t *,size_t);
    bool (*seal)(void *);
    bool (*activate)(void *);
    void (*abort)(void *);
} tv4_session_sink;
bool tv4_ota_session_install_guarded(tv4_ota_session *,const tv4_session_request *,const tv4_session_io *,
    const tv4_ota_resource_policy *,tv4_ota_resource_reader,void *,tv4_ota_resource_report *,
    const tv4_session_sink *);
/* Single owner, outside run only. Releases hash contexts and clears all state. */
void tv4_ota_session_reset(tv4_ota_session *);
