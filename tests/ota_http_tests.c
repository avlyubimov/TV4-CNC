#include "download.h"
#include "https.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *initial="https://github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0/manifest.json";
static const char *cdn="https://release-assets.githubusercontent.com/github-production-release-asset/1/uuid?sig=opaque%2Btoken";
static tv4_http_workspace workspace;
typedef struct {
    uint32_t now,open_delay,read_delay,consume_delay;
    unsigned opens,closes,reads,consumes;
    int status,read_error;
    uint32_t declared,body_bytes;
    bool fail_open,incomplete,chunked,encoded,bad_headers,cancel,reject_body,loop_redirect;
    bool ambiguous,no_framing,cancel_on_read;
    const char *redirect;
} mock;
static bool open_mock(void *context,const char *url,uint32_t timeout,tv4_http_response *r) {
    mock *m=context;assert(timeout==50);assert(tv4_http_url_allowed(url,m->opens>0));
    ++m->opens;m->now+=m->open_delay;r->identity_encoding=!m->encoded;
    if(m->fail_open)return false;
    r->status=m->status;r->bad_headers=m->bad_headers;
    if(m->redirect && (m->opens==1 || m->loop_redirect)) {r->status=302;strcpy(r->location,m->redirect);}
    r->chunked=m->chunked;r->length_known=!m->chunked;r->content_length=m->declared;
    if(m->ambiguous)r->chunked=r->length_known=true;
    if(m->no_framing)r->chunked=r->length_known=false;
    return true;
}
static int read_mock(void *context,uint8_t *buffer,size_t capacity) {
    mock *m=context;m->now+=m->read_delay;++m->reads;
    if(m->cancel_on_read)m->cancel=true;
    if(m->read_error)return m->read_error;
    if(m->reads==1) {assert(m->body_bytes<=capacity);memset(buffer,'x',m->body_bytes);return (int)m->body_bytes;}
    return 0;
}
static bool complete(void *context) {return !((mock *)context)->incomplete;}
static void close_mock(void *context) {++((mock *)context)->closes;}
static uint32_t now(void *context) {return ((mock *)context)->now;}
static bool cancelled(void *context) {return ((mock *)context)->cancel;}
static bool consume(void *context,const uint8_t *bytes,size_t size) {
    mock *m=context;assert(bytes && size==m->body_bytes);++m->consumes;m->now+=m->consume_delay;
    return !m->reject_body;
}
static tv4_http_request request(void) {
    return (tv4_http_request){.url=initial,.maximum_bytes=16384,.idle_timeout_ms=100,
        .total_timeout_ms=1000,.io_timeout_ms=50,.maximum_redirects=3,.trusted_time=true};
}
static mock healthy(void) {return (mock){.status=200,.declared=4,.body_bytes=4};}
static tv4_http_result run(mock *m,tv4_http_request r) {
    tv4_http_ops ops={m,open_mock,read_mock,complete,close_mock,now,cancelled};tv4_http_report report;
    tv4_http_result result=tv4_http_fetch(&r,&ops,&workspace,consume,m,&report);
    assert(m->opens==m->closes); /* Includes partially failed open and redirects. */
    return result;
}
static void urls(void) {
    assert(tv4_http_url_allowed(initial,false));assert(!tv4_http_url_allowed(cdn,false));assert(tv4_http_url_allowed(cdn,true));
    assert(tv4_http_url_allowed("https://objects.githubusercontent.com/github-production-release-asset-2e65be/1/a?sig=test",true));
    const char *bad[]={"http://github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0/manifest.json",
        "https://github.com.evil.invalid/path","https://github.com@evil.invalid/path",
        "https://user:secret@github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0/manifest.json",
        "https://github.com:443/avlyubimov/TV4-CNC/releases/download/v0.2.0/manifest.json",
        "https://github.com/other/TV4-CNC/releases/download/v0.2.0/manifest.json",
        "https://github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0/manifest.json?token=x",
        "https://github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0/manifest.json#fragment",
        "https://release-assets.githubusercontent.com.evil.invalid/github-production-release-asset/1/a",
        "https://release-assets.githubusercontent.com/other/1/a",
        "https://release-assets.githubusercontent.com/github-production-release-asset/1/a?x=%0d%0aHeader",
        "https://release-assets.githubusercontent.com/github-production-release-asset/1/a?x=%ZZ",
        "https://release-assets.githubusercontent.com/github-production-release-asset/1/a?x=%00",
        "https://github.com\\evil.invalid/path","https://127.0.0.1/path"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(!tv4_http_url_allowed(bad[i],true));
}
int main(void) {
    urls();mock m=healthy();assert(run(&m,request())==TV4_HTTP_OK && m.consumes==1);
    m=healthy();m.redirect=cdn;assert(run(&m,request())==TV4_HTTP_OK && m.opens==2 && m.consumes==1);
    m=healthy();m.redirect=cdn;m.loop_redirect=true;assert(run(&m,request())==TV4_HTTP_REDIRECT && m.opens==4 && !m.consumes);
    m=healthy();m.redirect="http://evil.invalid/";assert(run(&m,request())==TV4_HTTP_REDIRECT && !m.consumes);
    m=healthy();m.status=206;assert(run(&m,request())==TV4_HTTP_STATUS && !m.consumes);
    m=healthy();m.status=404;assert(run(&m,request())==TV4_HTTP_STATUS && !m.consumes);
    m=healthy();m.fail_open=true;assert(run(&m,request())==TV4_HTTP_OPEN && m.closes==1);
    m=healthy();m.encoded=true;assert(run(&m,request())==TV4_HTTP_ENCODING && !m.consumes);
    m=healthy();m.bad_headers=true;assert(run(&m,request())==TV4_HTTP_ENCODING && !m.consumes);
    m=healthy();m.declared=16385;assert(run(&m,request())==TV4_HTTP_LENGTH && !m.consumes);
    m=healthy();m.body_bytes=5;assert(run(&m,request())==TV4_HTTP_LENGTH && !m.consumes);
    m=healthy();m.body_bytes=3;assert(run(&m,request())==TV4_HTTP_LENGTH);
    m=healthy();m.ambiguous=true;assert(run(&m,request())==TV4_HTTP_ENCODING && !m.consumes);
    m=healthy();m.no_framing=true;assert(run(&m,request())==TV4_HTTP_ENCODING && !m.consumes);
    m=healthy();m.read_error=5000;assert(run(&m,request())==TV4_HTTP_LENGTH && !m.consumes);
    m=healthy();m.cancel_on_read=true;assert(run(&m,request())==TV4_HTTP_CANCELLED && !m.consumes);
    m=healthy();m.chunked=true;assert(run(&m,request())==TV4_HTTP_OK);
    m=healthy();m.incomplete=true;assert(run(&m,request())==TV4_HTTP_LENGTH);
    m=healthy();m.read_error=-1;assert(run(&m,request())==TV4_HTTP_IO);
    m=healthy();m.reject_body=true;assert(run(&m,request())==TV4_HTTP_CONSUMER);
    m=healthy();m.cancel=true;assert(run(&m,request())==TV4_HTTP_CANCELLED && !m.opens);
    m=healthy();m.read_delay=100;assert(run(&m,request())==TV4_HTTP_TIMEOUT && !m.consumes);
    m=healthy();m.open_delay=100;assert(run(&m,request())==TV4_HTTP_TIMEOUT && !m.consumes);
    m=healthy();m.consume_delay=100;assert(run(&m,request())==TV4_HTTP_TIMEOUT);
    m=healthy();m.now=UINT32_MAX-10;m.read_delay=6;assert(run(&m,request())==TV4_HTTP_OK);
    tv4_http_request r=request();r.total_timeout_ms=150;m=healthy();m.read_delay=90;assert(run(&m,r)==TV4_HTTP_TIMEOUT);
    r=request();r.trusted_time=false;m=healthy();assert(run(&m,r)==TV4_HTTP_UNTRUSTED_TIME && !m.opens);
    r=request();r.expected_bytes=5;m=healthy();assert(run(&m,r)==TV4_HTTP_LENGTH && !m.consumes);
    r=request();r.maximum_redirects=4;m=healthy();assert(run(&m,r)==TV4_HTTP_ARGUMENT && !m.opens);
    r=request();r.url="https://github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0/manifest.sig";
    m=healthy();assert(run(&m,r)==TV4_HTTP_ARGUMENT && !m.opens);r.maximum_bytes=2048;
    m=healthy();assert(run(&m,r)==TV4_HTTP_OK);
    tv4_http_report report;
    assert(tv4_https_read(NULL,NULL,&workspace,consume,&m,NULL,NULL,&report)==TV4_HTTP_UNAVAILABLE);
    printf("HTTPS policy/redirects/status/length/encoding/cancel/deadlines/cleanup PASS; workspace=%zu bytes (host)\n",sizeof(workspace));
}
