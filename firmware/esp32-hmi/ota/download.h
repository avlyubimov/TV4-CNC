#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define TV4_HTTP_URL_MAX 2048u
#define TV4_HTTP_BODY_MAX 0x300000u
#define TV4_HTTP_CHUNK 4096u
typedef enum { TV4_HTTP_OK,TV4_HTTP_ARGUMENT,TV4_HTTP_UNTRUSTED_TIME,
    TV4_HTTP_URL,TV4_HTTP_OPEN,TV4_HTTP_STATUS,TV4_HTTP_REDIRECT,
    TV4_HTTP_ENCODING,TV4_HTTP_LENGTH,TV4_HTTP_IO,TV4_HTTP_TIMEOUT,
    TV4_HTTP_CANCELLED,TV4_HTTP_CONSUMER,TV4_HTTP_UNAVAILABLE } tv4_http_result;
typedef struct {
    int status;bool length_known,chunked,identity_encoding,bad_headers;
    uint32_t content_length;
    char location[TV4_HTTP_URL_MAX+1];
} tv4_http_response;
typedef struct {
    const char *url;
    uint32_t maximum_bytes,expected_bytes; /* expected=0 for bounded metadata. */
    uint32_t idle_timeout_ms,total_timeout_ms,io_timeout_ms;
    unsigned maximum_redirects; /* 0..3 */
    bool trusted_time; /* Real local TLS time provider, not static HMI clock. */
} tv4_http_request;
typedef struct {
    char current_url[TV4_HTTP_URL_MAX+1];
    uint8_t chunk[TV4_HTTP_CHUNK];
    tv4_http_response response;
} tv4_http_workspace;
typedef struct { uint32_t received;unsigned redirects; } tv4_http_report;
typedef struct {
    void *context;
    /* open called with workspace response cleared. On open failure close still
     * runs, permitting cleanup of partially created backend resources. */
    bool (*open)(void *,const char *,uint32_t io_timeout_ms,tv4_http_response *);
    int (*read)(void *,uint8_t *,size_t);
    bool (*complete)(void *);
    void (*close)(void *);
    uint32_t (*now_ms)(void *);
    bool (*cancelled)(void *); /* Optional; called on same worker. */
} tv4_http_ops;
typedef bool (*tv4_http_consume)(void *,const uint8_t *,size_t);
/* Initial URLs: canonical TV4-CNC release paths. Redirects additionally allow
 * exact GitHub artifact CDN hosts/paths. No credentials/fragments/ports/HTTP. */
bool tv4_http_url_allowed(const char *url,bool redirect);
/* Sequential worker-only bounded fetch algorithm. No UI/Flash/activation.
 * Workspace ~8 KiB owned by caller. Never logs response URLs/tokens/headers. */
tv4_http_result tv4_http_fetch(const tv4_http_request *request,const tv4_http_ops *ops,
    tv4_http_workspace *workspace,tv4_http_consume consume,void *consumer,
    tv4_http_report *report);
