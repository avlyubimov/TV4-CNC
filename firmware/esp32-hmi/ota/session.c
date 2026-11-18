#include "session.h"
#include <limits.h>
#include <string.h>
typedef struct {
    tv4_ota_session *session;
    const tv4_session_request *request;
    const tv4_session_io *io;
    uint32_t started;
} operation;
static bool stop(operation *op) {
    tv4_session_result *r=&op->session->result;
    if(op->io->cancelled && op->io->cancelled(op->io->context)) {
        r->error=TV4_SESSION_CANCEL;return true;
    }
    if((uint32_t)(op->io->now_ms(op->io->context)-op->started)>=op->request->total_timeout_ms) {
        r->error=TV4_SESSION_TIMEOUT;return true;
    }
    return false;
}
static bool metadata(void *context,const uint8_t *bytes,size_t length) {
    operation *op=context;tv4_ota_session *s=op->session;
    if(stop(op) || !bytes || !length)return false;
    bool manifest=s->result.state==TV4_SESSION_MANIFEST;
    uint8_t *target=manifest?s->manifest:s->envelope;
    size_t *used=manifest?&s->manifest_length:&s->envelope_length;
    size_t capacity=manifest?sizeof(s->manifest):sizeof(s->envelope);
    if(length>capacity-*used)return false;
    memcpy(target+*used,bytes,length);*used+=length;return true;
}
static bool image(void *context,const uint8_t *bytes,size_t length) {
    operation *op=context;if(stop(op))return false;
    bool ok=tv4_image_feed(&op->session->image,bytes,length,op->io->now_ms(op->io->context));
    op->session->result.image_bytes=op->session->image.received;return ok;
}
static bool fetch(operation *op,const char *url,uint32_t maximum,uint32_t expected,tv4_http_consume consume) {
    if(stop(op))return false;
    uint32_t elapsed=op->io->now_ms(op->io->context)-op->started;
    if(elapsed>=op->request->total_timeout_ms) {op->session->result.error=TV4_SESSION_TIMEOUT;return false;}
    uint32_t remaining=op->request->total_timeout_ms-elapsed;
    uint32_t idle=op->request->idle_timeout_ms;if(idle>remaining)idle=remaining;
    uint32_t timeout=op->request->io_timeout_ms;if(timeout>idle)timeout=idle;
    tv4_http_request request={url,maximum,expected,idle,remaining,timeout,
        op->request->maximum_redirects,op->request->trusted_time};
    tv4_http_report report={0};
    op->session->result.http=op->io->fetch(op->io->context,&request,&op->session->http,consume,op,&report);
    if(stop(op))return false;
    if(op->session->result.http!=TV4_HTTP_OK) {
        if(op->session->result.http==TV4_HTTP_CANCELLED)op->session->result.error=TV4_SESSION_CANCEL;
        else if(op->session->result.http==TV4_HTTP_TIMEOUT)op->session->result.error=TV4_SESSION_TIMEOUT;
        else op->session->result.error=TV4_SESSION_TRANSPORT;
        return false;
    }
    return true;
}
static void clear_received(tv4_ota_session *s) {
    memset(s->manifest,0,sizeof(s->manifest));memset(s->envelope,0,sizeof(s->envelope));
    memset(&s->http,0,sizeof(s->http));memset(s->envelope_url,0,sizeof(s->envelope_url));
    s->manifest_length=s->envelope_length=0;
}
static bool failed(tv4_ota_session *s) {
    s->result.failed_stage=s->result.state;
    if(s->image.error!=TV4_IMAGE_NONE)s->result.image=s->image.error;
    if(s->result.error==TV4_SESSION_TRANSPORT && s->result.http==TV4_HTTP_CONSUMER && s->result.image!=TV4_IMAGE_NONE)
        s->result.error=TV4_SESSION_FORMAT;
    if(s->result.error==TV4_SESSION_FORMAT && s->result.image==TV4_IMAGE_TIMEOUT)
        s->result.error=TV4_SESSION_TIMEOUT;
    tv4_image_abort(&s->image,s->result.error==TV4_SESSION_CANCEL);
    tv4_image_close(&s->image);memset(&s->candidate,0,sizeof(s->candidate));
    clear_received(s);
    s->result.state=s->result.error==TV4_SESSION_CANCEL?TV4_SESSION_CANCELLED:TV4_SESSION_FAILED;
    return false;
}
void tv4_ota_session_reset(tv4_ota_session *s) {
    if(!s)return;
    tv4_image_close(&s->image);memset(s,0,sizeof(*s));
}
bool tv4_ota_session_run(tv4_ota_session *s,const tv4_session_request *r,const tv4_session_io *io) {
    if(!s || s->result.state!=TV4_SESSION_IDLE)return false;
    if(!r || !io || !io->fetch || !io->now_ms || !r->trust ||
       !r->idle_timeout_ms || r->total_timeout_ms<r->idle_timeout_ms || r->total_timeout_ms>INT32_MAX ||
       !r->io_timeout_ms || r->io_timeout_ms>10000 || r->io_timeout_ms>r->idle_timeout_ms ||
       r->maximum_redirects>3 || !tv4_http_url_allowed(r->manifest_url,false)) {
        s->result.error=TV4_SESSION_ARGUMENT;return failed(s);
    }
    const char *asset=strrchr(r->manifest_url,'/');
    if(!asset || strcmp(asset+1,"manifest.json")) {s->result.error=TV4_SESSION_ARGUMENT;return failed(s);}
    size_t prefix=(size_t)(asset+1-r->manifest_url);
    memcpy(s->envelope_url,r->manifest_url,prefix);memcpy(s->envelope_url+prefix,"manifest.sig",13);
    operation op={s,r,io,io->now_ms(io->context)};
    s->result.state=TV4_SESSION_MANIFEST;
    if(!fetch(&op,r->manifest_url,sizeof(s->manifest),0,metadata))return failed(s);
    s->result.state=TV4_SESSION_ENVELOPE;
    if(!fetch(&op,s->envelope_url,sizeof(s->envelope),0,metadata))return failed(s);
    s->result.state=TV4_SESSION_AUTHENTICATING;
    if(stop(&op))return failed(s);
    s->result.policy=tv4_ota_signed_manifest_read(s->manifest,s->manifest_length,s->envelope,s->envelope_length,r->trust,&s->candidate);
    if(stop(&op))return failed(s);
    if(s->result.policy!=TV4_OTA_OK) {s->result.error=TV4_SESSION_POLICY;return failed(s);}
    /* The selected release URL must agree with authenticated metadata. */
    /* asset points at the slash before manifest.json; find preceding v-tag. */
    const char *tag=asset;while(tag>r->manifest_url && tag[-1]!='/')--tag;
    if(*tag!='v' || (size_t)(asset-tag)!=strlen(s->candidate.release)+1 ||
       memcmp(tag+1,s->candidate.release,strlen(s->candidate.release))) {
        s->result.error=TV4_SESSION_POLICY;s->result.policy=TV4_OTA_WRONG_VERSION;return failed(s);
    }
    clear_received(s);
    if(stop(&op))return failed(s);
    if(!r->verify_image) {s->result.state=TV4_SESSION_CANDIDATE_READY;return true;}
    s->result.state=TV4_SESSION_IMAGE;
    if(stop(&op))return failed(s);
    if(!tv4_image_start(&s->image,&s->candidate,r->image_limits,io->now_ms(io->context))) {
        s->result.error=TV4_SESSION_FORMAT;return failed(s);
    }
    if(!fetch(&op,s->candidate.url,s->candidate.size_bytes,s->candidate.size_bytes,image))return failed(s);
    s->result.state=TV4_SESSION_FINISHING;
    if(stop(&op))return failed(s);
    bool ok=tv4_image_finish(&s->image,io->now_ms(io->context));
    if(stop(&op))return failed(s);
    if(!ok) {s->result.error=TV4_SESSION_FORMAT;return failed(s);}
    tv4_image_close(&s->image);memset(&s->http,0,sizeof(s->http));
    if(stop(&op))return failed(s);
    s->result.state=TV4_SESSION_IMAGE_VERIFIED;return true;
}
