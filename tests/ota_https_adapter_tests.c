/* Actual HTTPS adapter/portable timed fetch against SDK doubles. No network. */
#include "https.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *mode;
static const char *initial="https://github.com/avlyubimov/TV4-CNC/releases/download/v0.1.0-bench.2/manifest.json";
static struct fake_http_client {esp_http_client_config_t cfg;unsigned position,id;} client;
static unsigned inits,opens,closes,consumes,logs;
static uint32_t ticks;
static bool cancel_requested;
static char value[8194],location[2050],key[130];
static tv4_http_workspace workspace;
static int header(const char *name,const char *data) {
    esp_http_client_event_t event={.event_id=HTTP_EVENT_ON_HEADER,.header_key=name,.header_value=data,.user_data=client.cfg.user_data};
    return client.cfg.event_handler(&event);
}
static int padding(const char *name,size_t count) {
    assert(count<sizeof(value));memset(value,'x',count);value[count]=0;return header(name,value);
}
int64_t esp_timer_get_time(void) {return (int64_t)++ticks*1000;}
tv4_time_result __wrap_tv4_time_clock_read(tv4_time_sample *out) {
    *out=(tv4_time_sample){INT64_C(2000000000000000)+(int64_t)ticks*1000,(uint64_t)ticks*1000,TV4_TIME_LOCAL_CONFIRMED};
    return TV4_TIME_OK;
}
esp_err_t esp_crt_bundle_attach(void *arg) {(void)arg;return ESP_OK;}
void service_test_log(const char *tag,const char *format,...) {
    assert(!strcmp(tag,"tv4_ota_http") && !strstr(format,"url") && !strstr(format,"Location"));
    char message[256];va_list ap;va_start(ap,format);int n=vsnprintf(message,sizeof(message),format,ap);va_end(ap);
    assert(n>0 && (size_t)n<sizeof(message) && !strstr(message,"https://"));++logs;
}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *cfg) {
    ++inits;
    assert(cfg && cfg->transport_type==HTTP_TRANSPORT_OVER_SSL && cfg->crt_bundle_attach==esp_crt_bundle_attach);
    assert(!cfg->skip_cert_common_name_check && cfg->disable_auto_redirect && cfg->max_authorization_retries==-1);
    assert(cfg->auth_type==HTTP_AUTH_TYPE_NONE && !cfg->keep_alive_enable && cfg->timeout_ms==100);
    assert(cfg->buffer_size==4096 && cfg->buffer_size_tx>=512 && cfg->buffer_size_tx<=2080 && cfg->event_handler && cfg->user_data);
    if(inits==1)assert(cfg->buffer_size_tx==512);
    assert(!strcmp(cfg->user_agent,"TV4-CNC-OTA/1") && tv4_http_url_allowed(cfg->url,inits>1));
    if(!strcmp(mode,"init-fail"))return NULL;
    client=(struct fake_http_client){.cfg=*cfg,.id=inits};return &client;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t c,const char *name,const char *data) {
    assert(c==&client && !strcmp(name,"Accept-Encoding") && !strcmp(data,"identity"));
    return !strcmp(mode,"set-fail")?ESP_FAIL:ESP_OK;
}
esp_err_t esp_http_client_open(esp_http_client_handle_t c,int length) {
    assert(c==&client && !length);++opens;
    /* Both SDKs require the entire request line, including query, in TX. */
    const char *path=strchr(c->cfg.url+8,'/');assert(path);
    if(strlen(path)+15>=(size_t)c->cfg.buffer_size_tx)return ESP_FAIL;
    if(!strcmp(mode,"timeout"))ticks+=15000;
    return !strcmp(mode,"open-fail")?ESP_FAIL:ESP_OK;
}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t c) {
    assert(c==&client);
    if(!strcmp(mode,"headers-fail"))return -1;
    if(!strcmp(mode,"cancel"))cancel_requested=true;
    if(!strcmp(mode,"long-key")) {memset(key,'x',129);key[129]=0;return header(key,"x")==ESP_OK?4:-1;}
    if(!strcmp(mode,"large-value"))return padding("Unknown",8193)==ESP_OK?4:-1;
    if(!strcmp(mode,"large-total")) {
        for(unsigned i=0;i<5;i++)if(padding("Unknown",4000)!=ESP_OK)return -1;
    }
    if(client.id==1 && (!strcmp(mode,"github") || !strcmp(mode,"url-ceiling") || !strcmp(mode,"long-location") || !strcmp(mode,"duplicate-location") || !strcmp(mode,"bad-location"))) {
        /* Measured GitHub CSP=4178 bytes; real redirect response totals ~5.7KiB. */
        if(padding("Content-Security-Policy",4178)!=ESP_OK)return -1;
        if(padding("Vary",151)!=ESP_OK)return -1;
        if(padding("Unknown",400)!=ESP_OK)return -1;
        if(header("Location",!strcmp(mode,"bad-location")?"http://evil.invalid/":location)!=ESP_OK)return -1;
        if(!strcmp(mode,"duplicate-location") && header("Location",location)!=ESP_OK)return -1;
        return header("Content-Length","0")==ESP_OK?0:-1;
    }
    if(!strcmp(mode,"gzip"))return header("Content-Encoding","gzip")==ESP_OK?4:-1;
    if(!strcmp(mode,"duplicate-encoding")) {
        if(header("Content-Encoding","identity")!=ESP_OK)return -1;
        if(header("Content-Encoding","identity")!=ESP_OK)return -1;
    }
    if(!strcmp(mode,"bad-transfer"))return header("Transfer-Encoding","gzip")==ESP_OK?4:-1;
    if(!strcmp(mode,"invalid-length"))return header("Content-Length","3145729")==ESP_OK?4:-1;
    if(!strcmp(mode,"chunked") || !strcmp(mode,"ambiguous")) {
        if(header("Transfer-Encoding","chunked")!=ESP_OK)return -1;
        if(!strcmp(mode,"chunked"))return 0;
    }
    if(!strcmp(mode,"no-length"))return 0;
    if(header("Content-Length","4")!=ESP_OK)return -1;
    if(!strcmp(mode,"duplicate-length") && header("Content-Length","4")!=ESP_OK)return -1;
    return 4;
}
int esp_http_client_get_status_code(esp_http_client_handle_t c) {
    assert(c==&client);
    return client.id==1 && (!strcmp(mode,"github") || !strcmp(mode,"url-ceiling") || !strcmp(mode,"long-location") || !strcmp(mode,"duplicate-location") || !strcmp(mode,"bad-location"))?302:200;
}
bool esp_http_client_is_chunked_response(esp_http_client_handle_t c) {assert(c==&client);return !strcmp(mode,"chunked") || !strcmp(mode,"ambiguous");}
int esp_http_client_read(esp_http_client_handle_t c,char *out,int capacity) {
    assert(c==&client && capacity>=4);
    if(client.position++)return 0;
    memcpy(out,"test",4);return 4;
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t c) {assert(c==&client);return true;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c) {assert(c==&client);++closes;return ESP_OK;}
static bool cancelled(void *arg) {(void)arg;return cancel_requested;}
static bool consume(void *arg,const uint8_t *bytes,size_t length) {(void)arg;assert(length==4 && !memcmp(bytes,"test",4));++consumes;return true;}
int main(int argc,char **argv) {
    assert(argc==2);mode=argv[1];
    const char *prefix="https://release-assets.githubusercontent.com/github-production-release-asset/1/x?opaque=";
    size_t location_length=!strcmp(mode,"url-ceiling")?2048:!strcmp(mode,"long-location")?2049:905;
    size_t prefix_length=strlen(prefix);memcpy(location,prefix,prefix_length);memset(location+prefix_length,'x',location_length-prefix_length);location[location_length]=0;
    tv4_time_anchor anchor={0};tv4_time_sample clock;
    assert(__wrap_tv4_time_clock_read(&clock)==TV4_TIME_OK);
    const tv4_time_policy policy={true,false,1700000000,2300000000,60000,2000};
    assert(tv4_time_adopt(&anchor,&policy,&clock)==TV4_TIME_OK);
    if(!strcmp(mode,"stale-time"))tv4_time_forget(&anchor);
    tv4_http_request request={initial,16384,0,1000,20000,100,3,true};tv4_http_report report;
    tv4_http_result result=tv4_https_read(&request,&anchor,&workspace,consume,NULL,cancelled,NULL,&report);
    bool good=!strcmp(mode,"github") || !strcmp(mode,"url-ceiling") || !strcmp(mode,"small") || !strcmp(mode,"chunked");
    if(good)assert(result==TV4_HTTP_OK && consumes==1 && report.received==4 && !logs);
    else {assert(result!=TV4_HTTP_OK && !consumes);}
    if(!good) {
        tv4_http_result expected=TV4_HTTP_OPEN;
        if(!strcmp(mode,"bad-location"))expected=TV4_HTTP_REDIRECT;
        if(!strcmp(mode,"no-length") || !strcmp(mode,"ambiguous"))expected=TV4_HTTP_ENCODING;
        if(!strcmp(mode,"cancel"))expected=TV4_HTTP_CANCELLED;
        if(!strcmp(mode,"timeout"))expected=TV4_HTTP_TIMEOUT;
        if(!strcmp(mode,"stale-time"))expected=TV4_HTTP_UNTRUSTED_TIME;
        assert(result==expected);
        if(expected==TV4_HTTP_OPEN)assert(logs==1);
    }
    assert(closes==inits-(!strcmp(mode,"init-fail")?1u:0u));
    if(!strcmp(mode,"github"))assert(inits==2 && opens==2 && report.redirects==1);
    if(!strcmp(mode,"stale-time"))assert(!inits && !opens && !closes);
    printf("actual HTTPS adapter: %s result=%u init=%u cleanup=%u PASS\n",mode,(unsigned)result,inits,closes);
    return 0;
}
