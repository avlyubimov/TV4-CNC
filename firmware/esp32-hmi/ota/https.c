#include "https.h"
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include <string.h>

#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
/* No global client/header state. The caller owns the workspace throughout. */
typedef struct {
    esp_http_client_handle_t client;tv4_http_response *response;
    bool (*cancelled)(void *);void *cancel_context;
    size_t header_bytes;
    bool location_seen,length_seen,transfer_seen,encoding_seen;
} bridge;
static size_t short_length(const char *s,size_t maximum) {
    if(!s)return maximum+1;
    for(size_t i=0;i<=maximum;i++)if(!s[i])return i;
    return maximum+1;
}
static int header_compare(const char *a,const char *b) {
    while(*a && *b) {
        unsigned char x=(unsigned char)*a++,y=(unsigned char)*b++;
        if(x>='A'&&x<='Z')x+='a'-'A';
        if(y>='A'&&y<='Z')y+='a'-'A';
        if(x!=y)return x-y;
    }
    return (unsigned char)*a-(unsigned char)*b;
}
static esp_err_t headers(esp_http_client_event_t *event) {
    if(event->event_id!=HTTP_EVENT_ON_HEADER)return ESP_OK;
    bridge *b=event->user_data;tv4_http_response *r=b->response;
    size_t key=short_length(event->header_key,128),value=short_length(event->header_value,4096);
    if(key>128 || value>4096 || key+value+4>4096-b->header_bytes) {r->bad_headers=true;return ESP_FAIL;}
    b->header_bytes+=key+value+4;
    if(!header_compare(event->header_key,"Location")) {
        if(b->location_seen || !value || value>TV4_HTTP_URL_MAX)r->bad_headers=true;
        else memcpy(r->location,event->header_value,value+1);
        b->location_seen=true;
    } else if(!header_compare(event->header_key,"Content-Length")) {
        if(b->length_seen || !value || value>10)r->bad_headers=true;
        else {
            uint32_t length=0;
            for(size_t i=0;i<value;i++) {
                unsigned char c=(unsigned char)event->header_value[i];
                if(c<'0' || c>'9' || length>(TV4_HTTP_BODY_MAX-(uint32_t)(c-'0'))/10) {r->bad_headers=true;break;}
                length=length*10+(uint32_t)(c-'0');
            }
            r->content_length=length;r->length_known=true;
        }
        b->length_seen=true;
    } else if(!header_compare(event->header_key,"Transfer-Encoding")) {
        if(b->transfer_seen || header_compare(event->header_value,"chunked"))r->bad_headers=true;
        else r->chunked=true;
        b->transfer_seen=true;
    } else if(!header_compare(event->header_key,"Content-Encoding")) {
        if(b->encoding_seen || header_compare(event->header_value,"identity"))r->bad_headers=true;
        b->encoding_seen=true;
    }
    return r->bad_headers?ESP_FAIL:ESP_OK;
}
static bool open_client(void *context,const char *url,uint32_t timeout,tv4_http_response *response) {
    bridge *b=context;b->response=response;b->header_bytes=0;
    b->location_seen=b->length_seen=b->transfer_seen=b->encoding_seen=false;
    response->identity_encoding=true;
    esp_http_client_config_t cfg={.url=url,.timeout_ms=(int)timeout,
        .transport_type=HTTP_TRANSPORT_OVER_SSL,.crt_bundle_attach=esp_crt_bundle_attach,
        .skip_cert_common_name_check=false,.disable_auto_redirect=true,
        .max_authorization_retries=-1,.auth_type=HTTP_AUTH_TYPE_NONE,
        .buffer_size=4096,.buffer_size_tx=512,.event_handler=headers,.user_data=b,
        .user_agent="TV4-CNC-OTA/1",.keep_alive_enable=false};
    b->client=esp_http_client_init(&cfg);if(!b->client)return false;
    if(esp_http_client_set_header(b->client,"Accept-Encoding","identity")!=ESP_OK ||
       esp_http_client_open(b->client,0)!=ESP_OK || esp_http_client_fetch_headers(b->client)<0)return false;
    response->status=esp_http_client_get_status_code(b->client);
    if(esp_http_client_is_chunked_response(b->client)!=response->chunked)response->bad_headers=true;
    return !response->bad_headers;
}
static int read_client(void *context,uint8_t *bytes,size_t length) {
    return esp_http_client_read(((bridge *)context)->client,(char *)bytes,(int)length);
}
static bool complete_client(void *context) {return esp_http_client_is_complete_data_received(((bridge *)context)->client);}
static void close_client(void *context) {
    bridge *b=context;if(b->client) {esp_http_client_cleanup(b->client);b->client=NULL;}
}
static uint32_t now(void *context) {(void)context;return (uint32_t)(esp_timer_get_time()/1000);}
static bool cancelled(void *context) {bridge *b=context;return b->cancelled && b->cancelled(b->cancel_context);}
#endif
#endif
tv4_http_result tv4_https_read(const tv4_http_request *r,tv4_http_workspace *w,
    tv4_http_consume consume,void *consumer,bool (*cancel_cb)(void *),void *cancel_context,tv4_http_report *report) {
#if defined(ESP_PLATFORM) && CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    bridge b={.cancelled=cancel_cb,.cancel_context=cancel_context};
    tv4_http_ops ops={&b,open_client,read_client,complete_client,close_client,now,cancelled};
    return tv4_http_fetch(r,&ops,w,consume,consumer,report);
#else
    (void)r;(void)w;(void)consume;(void)consumer;(void)cancel_cb;(void)cancel_context;
    if(report) {*report=(tv4_http_report){0};}
    return TV4_HTTP_UNAVAILABLE;
#endif
}
