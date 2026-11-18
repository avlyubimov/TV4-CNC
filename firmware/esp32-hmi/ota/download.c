#include "download.h"
#include "manifest.h"
#include <limits.h>
#include <string.h>
static size_t bounded_length(const char *s,size_t max) {
    if(!s)return max+1;
    for(size_t i=0;i<=max;i++)if(!s[i])return i;
    return max+1;
}
static int hex(unsigned char c) {
    if(c>='0' && c<='9')return c-'0';
    if(c>='a' && c<='f')return c-'a'+10;
    if(c>='A' && c<='F')return c-'A'+10;
    return -1;
}
static bool release_path(const char *path) {
    const char prefix[]="/avlyubimov/TV4-CNC/releases/download/v";
    if(strncmp(path,prefix,sizeof(prefix)-1))return false;
    const char *tag=path+sizeof(prefix)-1,*slash=strchr(tag,'/');
    if(!slash || slash==tag || (size_t)(slash-tag)>64)return false;
    char version[65];memcpy(version,tag,(size_t)(slash-tag));version[slash-tag]=0;int cmp;
    if(!tv4_ota_version_compare(version,version,&cmp))return false;
    const char *asset=slash+1;
    if(!strcmp(asset,"manifest.json") || !strcmp(asset,"manifest.sig"))return true;
    const char image[]="tv4-hmi-esp32s3-v";size_t n=strlen(asset);
    if(strncmp(asset,image,sizeof(image)-1) || n<sizeof(image)+4 || strcmp(asset+n-4,".bin"))return false;
    size_t length=n-(sizeof(image)-1)-4;
    if(!length || length>31)return false;
    memcpy(version,asset+sizeof(image)-1,length);version[length]=0;
    return tv4_ota_version_compare(version,version,&cmp);
}
bool tv4_http_url_allowed(const char *url,bool redirect) {
    size_t n=bounded_length(url,TV4_HTTP_URL_MAX);
    if(n<9 || n>TV4_HTTP_URL_MAX || strncmp(url,"https://",8))return false;
    for(size_t i=0;i<n;i++) {
        unsigned char c=(unsigned char)url[i];if(c<=0x20 || c>=0x7f || c=='\\' || c=='#')return false;
        if(c=='%') {
            if(i+2>=n)return false;
            int a=hex((unsigned char)url[i+1]),b=hex((unsigned char)url[i+2]);
            if(a<0 || b<0)return false;
            unsigned value=(unsigned)(a*16+b);
            if(value<0x20 || value==0x7f || value=='\\')return false;
            i+=2;
        }
    }
    const char *host=url+8,*path=strchr(host,'/');if(!path)return false;
    size_t host_length=(size_t)(path-host);
    if(host_length==10 && !memcmp(host,"github.com",10))return release_path(path);
    if(!redirect)return false;
    if(host_length==36 && !memcmp(host,"release-assets.githubusercontent.com",36))
        return !strncmp(path,"/github-production-release-asset/",33);
    if(host_length==29 && !memcmp(host,"objects.githubusercontent.com",29))
        return !strncmp(path,"/github-production-release-asset-",33);
    return false;
}
static tv4_http_result budget(const tv4_http_request *r,const tv4_http_ops *o,uint32_t start,uint32_t progress) {
    if(o->cancelled && o->cancelled(o->context))return TV4_HTTP_CANCELLED;
    uint32_t now=o->now_ms(o->context);
    if((uint32_t)(now-start)>=r->total_timeout_ms || (uint32_t)(now-progress)>=r->idle_timeout_ms)return TV4_HTTP_TIMEOUT;
    return TV4_HTTP_OK;
}
static bool redirection(int status) {return status==301 || status==302 || status==303 || status==307 || status==308;}
tv4_http_result tv4_http_fetch(const tv4_http_request *r,const tv4_http_ops *o,
    tv4_http_workspace *w,tv4_http_consume consume,void *consumer,tv4_http_report *report) {
    if(report)memset(report,0,sizeof(*report));
    if(!r || !o || !w || !report || !consume || !o->open || !o->read || !o->complete || !o->close || !o->now_ms ||
       !r->maximum_bytes || r->maximum_bytes>TV4_HTTP_BODY_MAX || r->expected_bytes>r->maximum_bytes ||
       !r->idle_timeout_ms || r->idle_timeout_ms>INT32_MAX || r->total_timeout_ms<r->idle_timeout_ms ||
       r->total_timeout_ms>INT32_MAX || !r->io_timeout_ms || r->io_timeout_ms>10000 ||
       r->io_timeout_ms>r->idle_timeout_ms || r->maximum_redirects>3)return TV4_HTTP_ARGUMENT;
    if(!r->trusted_time)return TV4_HTTP_UNTRUSTED_TIME;
    if(!tv4_http_url_allowed(r->url,false))return TV4_HTTP_URL;
    const char *asset=strrchr(r->url,'/')+1;
    if((!strcmp(asset,"manifest.json") && r->maximum_bytes>16384) ||
       (!strcmp(asset,"manifest.sig") && r->maximum_bytes>2048) ||
       (strcmp(asset,"manifest.json") && strcmp(asset,"manifest.sig") && !r->expected_bytes))return TV4_HTTP_ARGUMENT;
    memmove(w->current_url,r->url,strlen(r->url)+1);
    uint32_t start=o->now_ms(o->context),progress=start;
    for(;;) {
        tv4_http_result result=budget(r,o,start,progress);if(result!=TV4_HTTP_OK)return result;
        memset(&w->response,0,sizeof(w->response));
        if(!o->open(o->context,w->current_url,r->io_timeout_ms,&w->response)) {o->close(o->context);return TV4_HTTP_OPEN;}
        result=budget(r,o,start,progress);
        if(result!=TV4_HTTP_OK) {o->close(o->context);return result;}
        tv4_http_response *h=&w->response;
        if(h->bad_headers) {o->close(o->context);return TV4_HTTP_ENCODING;}
        if(redirection(h->status)) {
            bool allowed=report->redirects<r->maximum_redirects && tv4_http_url_allowed(h->location,true);
            o->close(o->context);
            if(!allowed)return TV4_HTTP_REDIRECT;
            strcpy(w->current_url,h->location);++report->redirects;continue;
        }
        if(h->status!=200) {o->close(o->context);return TV4_HTTP_STATUS;}
        if(!h->identity_encoding || (h->length_known && h->chunked) || (!h->length_known && !h->chunked)) {
            o->close(o->context);return TV4_HTTP_ENCODING;
        }
        if(h->length_known && (!h->content_length || h->content_length>r->maximum_bytes ||
           (r->expected_bytes && h->content_length!=r->expected_bytes))) {
            o->close(o->context);return TV4_HTTP_LENGTH;
        }
        for(;;) {
            result=budget(r,o,start,progress);if(result!=TV4_HTTP_OK)break;
            int n=o->read(o->context,w->chunk,sizeof(w->chunk));
            result=budget(r,o,start,progress);if(result!=TV4_HTTP_OK)break;
            if(n<0) {result=TV4_HTTP_IO;break;}
            if(n==0) {
                if(!o->complete(o->context) || !report->received ||
                   (h->length_known && report->received!=h->content_length) ||
                   (r->expected_bytes && report->received!=r->expected_bytes))result=TV4_HTTP_LENGTH;
                break;
            }
            if((unsigned)n>TV4_HTTP_CHUNK || (uint32_t)n>r->maximum_bytes-report->received ||
               (r->expected_bytes && (uint32_t)n>r->expected_bytes-report->received) ||
               (h->length_known && (uint32_t)n>h->content_length-report->received)) {result=TV4_HTTP_LENGTH;break;}
            report->received+=(uint32_t)n;
            if(!consume(consumer,w->chunk,(size_t)n)) {result=TV4_HTTP_CONSUMER;break;}
            result=budget(r,o,start,progress);if(result!=TV4_HTTP_OK)break;
            progress=o->now_ms(o->context);
        }
        o->close(o->context);return result;
    }
}
