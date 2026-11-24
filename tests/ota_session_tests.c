#include "session.h"
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static tv4_ota_session session;
static EVP_PKEY *key;
static unsigned char *public_key;
static tv4_ota_context trust;
static uint8_t binary[480];
static char manifest[4096],envelope[256];
static size_t manifest_length,envelope_length;
static const char url[]="https://github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0/manifest.json";
static void put32(uint8_t *p,uint32_t n) {for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(i*8));}
static void sign_manifest(void) {
    unsigned char der[80],b64[112];size_t n=sizeof(der);
    EVP_MD_CTX *ctx=EVP_MD_CTX_new();assert(ctx);
    assert(EVP_DigestSignInit(ctx,NULL,EVP_sha256(),NULL,key)==1);
    assert(EVP_DigestSign(ctx,der,&n,(const unsigned char *)manifest,manifest_length)==1);
    EVP_MD_CTX_free(ctx);assert(EVP_EncodeBlock(b64,der,(int)n)>0);
    int count=snprintf(envelope,sizeof(envelope),"{\"key_id\":\"test-only\",\"algorithm\":\"ECDSA-P256-SHA256\",\"signature\":\"%s\"}",b64);
    assert(count>0 && (size_t)count<sizeof(envelope));envelope_length=(size_t)count;
}
static void fixture(void) {
    tv4_ota_session_reset(&session);
    memset(binary,0,sizeof(binary));binary[0]=0xe9;binary[1]=2;binary[2]=2;binary[3]=0x4f;
    binary[12]=9;binary[17]=99;binary[23]=1;
    put32(binary+24,0x3c000020);put32(binary+28,320);
    put32(binary+32,0xabcd5432);strcpy((char *)binary+48,"0.2.0");strcpy((char *)binary+80,"tv4_hmi");
    for(size_t i=288;i<352;i++)binary[i]=(uint8_t)i;
    put32(binary+352,0x3fc90000);put32(binary+356,80);
    for(size_t i=360;i<440;i++)binary[i]=(uint8_t)(i*3);
    uint8_t check=0xef;
    for(size_t i=32;i<352;i++)check^=binary[i];
    for(size_t i=360;i<440;i++)check^=binary[i];
    binary[447]=check;assert(tv4_ota_sha256(binary,448,binary+448));
    uint8_t digest[32];char hex[65];assert(tv4_ota_sha256(binary,sizeof(binary),digest));
    for(unsigned i=0;i<32;i++)snprintf(hex+i*2,3,"%02x",digest[i]);
    int n=snprintf(manifest,sizeof(manifest),
        "{\"schema_version\":1,\"release_version\":\"0.2.0\",\"channel\":\"stable\","
        "\"source_commit\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
        "\"signing\":{\"key_id\":\"test-only\",\"algorithm\":\"ECDSA-P256-SHA256\"},"
        "\"protocol\":{\"min\":\"1.0.0\",\"max\":\"1.0.0\"},\"components\":[{"
        "\"name\":\"hmi\",\"version\":\"0.2.0\",\"hardware_id\":\"waveshare-esp32s3-touch-lcd-7\","
        "\"hardware_revision\":\"1.2\",\"chip\":\"esp32s3\",\"profile\":\"rev12\","
        "\"layout_id\":\"tv4-16m-ab-v1\",\"minimum_installed_version\":\"0.1.0\","
        "\"minimum_bootloader_version\":\"0.1.0\",\"installable\":true,\"size_bytes\":480,\"sha256\":\"%s\","
        "\"url\":\"https://github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0/tv4-hmi-esp32s3-v0.2.0.bin\"}],"
        "\"release_notes\":\"Test fixture only\"}",hex);
    assert(n>0 && (size_t)n<sizeof(manifest));manifest_length=(size_t)n;sign_manifest();
}
typedef struct {
    unsigned calls,opens,closes,index,fail_call,cancel_call,consumer_call;
    uint32_t now,read_delay;
    bool cancel,expire_auth,cancel_auth;unsigned auth_time_checks,auth_cancel_checks;
    const uint8_t *body;size_t length,offset,cut,disconnect_after;
    tv4_session_state stages[3];
} server;
static bool open_stream(void *context,const char *address,uint32_t timeout,tv4_http_response *response) {
    server *s=context;(void)timeout;s->opens++;
    if(s->calls==s->fail_call)return false;
    if(strstr(address,"manifest.json")) {s->body=(const uint8_t *)manifest;s->length=manifest_length;s->index=0;}
    else if(strstr(address,"manifest.sig")) {s->body=(const uint8_t *)envelope;s->length=envelope_length;s->index=1;}
    else {s->body=binary;s->length=sizeof(binary);s->index=2;}
    s->offset=0;response->status=200;response->identity_encoding=true;response->length_known=true;
    response->content_length=(uint32_t)s->length;return true;
}
static int read_stream(void *context,uint8_t *bytes,size_t capacity) {
    server *s=context;s->now+=s->read_delay;
    if(s->index==2 && s->disconnect_after && s->offset>=s->disconnect_after)return -1;
    if(s->calls==s->cancel_call)s->cancel=true;
    size_t end=s->length;if(s->index==2 && s->cut<end)end=s->cut;
    size_t n=s->offset<end?end-s->offset:0;
    if(n>17)n=17;
    assert(n<=capacity);memcpy(bytes,s->body+s->offset,n);s->offset+=n;return (int)n;
}
static bool complete(void *context) {server *s=context;return s->offset==s->length;}
static void close_stream(void *context) {((server *)context)->closes++;}
static uint32_t now(void *context) {
    server *s=context;
    if(s->expire_auth && session.result.state==TV4_SESSION_AUTHENTICATING && ++s->auth_time_checks==2)s->now+=10000;
    return s->now;
}
static bool cancel(void *context) {
    server *s=context;
    if(s->cancel_auth && session.result.state==TV4_SESSION_AUTHENTICATING && ++s->auth_cancel_checks==2)s->cancel=true;
    return s->cancel;
}
static tv4_http_result fetch(void *context,const tv4_http_request *request,tv4_http_workspace *workspace,
    tv4_http_consume consume,void *consumer,tv4_http_report *report) {
    server *s=context;assert(s->calls<3);s->stages[s->calls]=session.result.state;s->calls++;
    if(s->calls==s->consumer_call) {uint8_t big[4096]={0};for(unsigned i=0;i<5;i++)if(!consume(consumer,big,sizeof(big)))return TV4_HTTP_CONSUMER;}
    tv4_http_ops ops={s,open_stream,read_stream,complete,close_stream,now,cancel};
    return tv4_http_fetch(request,&ops,workspace,consume,consumer,report);
}
static tv4_session_request request(void) {
    return (tv4_session_request){url,&trust,{1000,10000,2,true},1000,10000,100,3,true,true};
}
static server healthy(void) {server s={0};s.cut=sizeof(binary);return s;}
static bool run(server *s,tv4_session_request r) {
    tv4_session_io io={s,fetch,now,cancel};bool ok=tv4_ota_session_run(&session,&r,&io);
    assert(s->opens==s->closes);assert(!session.image.whole_hash.context && !session.image.body_hash.context);
    assert(!session.manifest_length && !session.envelope_length);
    for(size_t i=0;i<sizeof(session.manifest);i++)assert(!session.manifest[i]);
    if(!ok)assert(!session.candidate.size_bytes && !session.candidate.url[0]);
    return ok;
}
typedef struct {
    server *server;
    unsigned reads,fail_at;
    bool unavailable,stack_failure,cancel_after_failure,expire_after_failure;
    bool failed_active,failed_after_close,failed_with_candidate;
} resource_fixture;
/* Synthetic test policy; never a production admission threshold. */
static const tv4_ota_resource_policy resource_policy={true,32768,8192,65536,32768,2048};
static bool resource_read(void *context,tv4_ota_resource_sample *out) {
    resource_fixture *f=context;f->reads++;
    *out=(tv4_ota_resource_sample){65536,32768,1024*1024,512*1024,8192};
    if(f->reads!=f->fail_at)return true;
    f->failed_active=f->server->opens>f->server->closes;
    f->failed_after_close=f->server->closes && f->server->closes==f->server->opens;
    f->failed_with_candidate=session.candidate.size_bytes!=0;
    if(f->cancel_after_failure)f->server->cancel=true;
    if(f->expire_after_failure)f->server->now+=10000;
    if(f->unavailable)return false;
    if(f->stack_failure)out->stack_watermark=resource_policy.stack_floor-1;
    else {out->internal_free=resource_policy.internal_free_floor-1;out->internal_largest=8192;}
    return true;
}
static bool guarded(server *s,tv4_session_request r,const tv4_ota_resource_policy *p,
    tv4_ota_resource_reader reader,resource_fixture *f,tv4_ota_resource_report *report) {
    tv4_session_io io={s,fetch,now,cancel};
    bool ok=tv4_ota_session_run_guarded(&session,&r,&io,p,reader,f,report);
    assert(s->opens==s->closes && !session.image.whole_hash.context && !session.image.body_hash.context);
    assert(!session.manifest_length && !session.envelope_length);
    for(size_t i=0;i<sizeof(session.http);i++)assert(!((const unsigned char *)&session.http)[i]);
    if(!ok)assert(!session.candidate.size_bytes && !session.candidate.url[0]);
    return ok;
}
static void resource_regressions(void) {
    fixture();server s=healthy();resource_fixture f={.server=&s};tv4_ota_resource_report report;
    assert(guarded(&s,request(),&resource_policy,resource_read,&f,&report));
    assert(report.result==TV4_RESOURCE_OK && report.checks==f.reads && s.calls==3);
    unsigned boundaries=f.reads,active=0,closed=0,candidate=0;assert(boundaries>20);
    /* Reuse the SAME signed bytes: DER/base64 length can vary if re-signed. */
    for(unsigned fail=1;fail<=boundaries;fail++) {
        tv4_ota_session_reset(&session);s=healthy();f=(resource_fixture){.server=&s,.fail_at=fail};
        assert(!guarded(&s,request(),&resource_policy,resource_read,&f,&report));
        assert(session.result.error==TV4_SESSION_RESOURCE && session.result.state==TV4_SESSION_FAILED);
        assert(report.result==TV4_RESOURCE_INTERNAL_FREE && report.checks==fail && f.reads==fail);
        if(f.failed_active) {active++;assert(session.result.http==TV4_HTTP_CONSUMER);}
        if(f.failed_after_close)closed++;
        if(f.failed_with_candidate)candidate++;
    }
    assert(active && closed && candidate);
    tv4_ota_session_reset(&session);s=healthy();f=(resource_fixture){.server=&s,.fail_at=boundaries,.stack_failure=true};
    assert(!guarded(&s,request(),&resource_policy,resource_read,&f,&report));
    assert(report.result==TV4_RESOURCE_STACK && session.result.failed_stage==TV4_SESSION_FINISHING);
    tv4_ota_session_reset(&session);s=healthy();f=(resource_fixture){.server=&s,.fail_at=2,.unavailable=true};
    assert(!guarded(&s,request(),&resource_policy,resource_read,&f,&report));
    assert(report.result==TV4_RESOURCE_UNAVAILABLE && !report.sample.internal_free);
    /* The first resource failure remains sticky even if cleanup frees memory,
       or cancellation/deadline becomes visible after the failure. */
    tv4_ota_session_reset(&session);s=healthy();f=(resource_fixture){.server=&s,.fail_at=2,.cancel_after_failure=true,.expire_after_failure=true};
    assert(!guarded(&s,request(),&resource_policy,resource_read,&f,&report));
    assert(session.result.error==TV4_SESSION_RESOURCE && report.checks==2 && f.reads==2);
    tv4_ota_session_reset(&session);s=healthy();f=(resource_fixture){.server=&s};
    tv4_ota_resource_policy p=resource_policy;p.reviewed=false;
    assert(!guarded(&s,request(),&p,resource_read,&f,&report));
    assert(report.result==TV4_RESOURCE_NOT_REVIEWED && !f.reads && !s.calls);
    tv4_ota_session_reset(&session);s=healthy();
    assert(!guarded(&s,request(),NULL,resource_read,&f,&report));
    assert(report.result==TV4_RESOURCE_ARGUMENT && !s.calls);
    tv4_ota_session_reset(&session);s=healthy();
    assert(!guarded(&s,request(),&resource_policy,resource_read,&f,NULL));
    assert(session.result.error==TV4_SESSION_ARGUMENT && !s.calls);
    tv4_ota_session_reset(&session);s=healthy();
    assert(!guarded(&s,request(),&resource_policy,NULL,&f,&report));
    assert(report.result==TV4_RESOURCE_ARGUMENT && !s.calls);
    tv4_ota_session_reset(&session);s=healthy();
    assert(!guarded(&s,request(),&resource_policy,tv4_ota_resource_sample_current,&f,&report));
    assert(report.result==TV4_RESOURCE_UNAVAILABLE && !s.calls);
    tv4_ota_session_reset(&session);s=healthy();s.cancel=true;f=(resource_fixture){.server=&s,.fail_at=1};
    assert(!guarded(&s,request(),&resource_policy,resource_read,&f,&report));
    assert(session.result.state==TV4_SESSION_CANCELLED && !report.checks && !s.calls);
    tv4_ota_session_reset(&session);s=healthy();f=(resource_fixture){.server=&s};
    tv4_session_request r=request();r.verify_image=false;
    assert(guarded(&s,r,&resource_policy,resource_read,&f,&report) && s.calls==2);
    assert(session.result.state==TV4_SESSION_CANDIDATE_READY && report.result==TV4_RESOURCE_OK);
    /* Active/terminal run is rejected without changing the prior report/result. */
    unsigned saved=report.checks;
    assert(!tv4_ota_session_run_guarded(&session,NULL,NULL,NULL,NULL,NULL,&report));
    assert(session.result.state==TV4_SESSION_CANDIDATE_READY && report.checks==saved);
    tv4_ota_session_reset(&session);s=healthy();f=(resource_fixture){.server=&s,.fail_at=saved,.stack_failure=true};
    assert(!guarded(&s,r,&resource_policy,resource_read,&f,&report));
    assert(report.result==TV4_RESOURCE_STACK && session.result.failed_stage==TV4_SESSION_AUTHENTICATING);
    for(unsigned kind=0;kind<2;kind++) {
        tv4_ota_session_reset(&session);s=healthy();s.expire_auth=kind==0;s.cancel_auth=kind==1;
        f=(resource_fixture){.server=&s};
        assert(!guarded(&s,request(),&resource_policy,resource_read,&f,&report));
        assert(session.result.error==(kind?TV4_SESSION_CANCEL:TV4_SESSION_TIMEOUT));
        assert(report.result==TV4_RESOURCE_OK && session.result.failed_stage==TV4_SESSION_AUTHENTICATING);
    }
    fixture();s=healthy();manifest[0]='[';f=(resource_fixture){.server=&s};
    assert(!guarded(&s,request(),&resource_policy,resource_read,&f,&report));
    assert(session.result.error==TV4_SESSION_POLICY && session.result.policy==TV4_OTA_BAD_SIGNATURE);
    assert(report.result==TV4_RESOURCE_OK);
    printf("Guarded signed session: resource fault at all %u boundaries, active/cleanup/authenticated candidate revocation PASS\n",boundaries);
}
typedef struct {server *server;unsigned begin,write,seal,activate,abort,fail;bool selected,cancel_seal;size_t used,cancel_after;uint8_t bytes[480];} install_fixture;
static bool install_begin(void *v,const tv4_ota_candidate *c) {
    install_fixture *f=v;f->begin++;assert(f->server->calls==2 && c->size_bytes==480 && !strcmp(c->version,"0.2.0"));return f->fail!=1;
}
static bool install_write(void *v,const uint8_t *p,size_t n) {
    install_fixture *f=v;f->write++;assert(f->begin==1 && !f->seal && !f->activate && n<=480-f->used);
    if(f->fail==2)return false;
    memcpy(f->bytes+f->used,p,n);f->used+=n;
    if(f->cancel_after && f->used>=f->cancel_after)f->server->cancel=true;
    return true;
}
static bool install_seal(void *v) {
    install_fixture *f=v;f->seal++;assert(f->used==480 && !memcmp(f->bytes,binary,480));
    assert(f->server->closes==3 && session.result.state==TV4_SESSION_FINISHING && !session.image.whole_hash.context && !session.image.body_hash.context);
    if(f->cancel_seal)f->server->cancel=true;
    return f->fail!=3;
}
static bool install_activate(void *v) {install_fixture *f=v;f->activate++;assert(f->seal==1);if(f->fail==4)return false;f->selected=true;return true;}
static void install_abort(void *v) {install_fixture *f=v;f->abort++;assert(!f->selected);}
static bool installing(server *s,install_fixture *f,resource_fixture *rf,tv4_ota_resource_report *report) {
    f->server=s;tv4_session_io io={s,fetch,now,cancel};tv4_session_request r=request();
    tv4_session_sink sink={f,install_begin,install_write,install_seal,install_activate,install_abort};
    bool ok=tv4_ota_session_install_guarded(&session,&r,&io,&resource_policy,resource_read,rf,report,&sink);
    assert(s->opens==s->closes && !session.image.whole_hash.context && !session.image.body_hash.context);return ok;
}
static void installation_regressions(void) {
    fixture();server s=healthy();install_fixture f={0};resource_fixture rf={.server=&s};tv4_ota_resource_report report;
    assert(installing(&s,&f,&rf,&report));assert(session.result.state==TV4_SESSION_INSTALLED && f.selected && f.activate==1 && !f.abort);
    unsigned checks=report.checks;
    for(unsigned legacy=0;legacy<2;legacy++) {
        fixture();char *at=strstr(manifest,"\"installable\":true,");assert(at);
        if(legacy)memmove(at,at+strlen("\"installable\":true,"),strlen(at+strlen("\"installable\":true,"))+1);
        else { /* Replace with a signed false, not an unauthenticated edit. */
            const char *tail=at+strlen("\"installable\":true,");char saved[4096];strcpy(saved,tail);
            strcpy(at,"\"installable\":false,");strcat(at,saved);
        }
        manifest_length=strlen(manifest);sign_manifest();s=healthy();f=(install_fixture){0};rf=(resource_fixture){.server=&s};
        assert(!installing(&s,&f,&rf,&report) && session.result.error==TV4_SESSION_POLICY && session.result.policy==TV4_OTA_NOT_INSTALLABLE);
        assert(s.calls==2 && !f.begin && !f.write && !f.seal && !f.activate && f.abort==1);
        tv4_ota_session_reset(&session);s=healthy();
        assert(run(&s,request()) && session.result.state==TV4_SESSION_IMAGE_VERIFIED && !session.candidate.installable);
    }
    for(unsigned phase=1;phase<=4;phase++) {
        fixture();s=healthy();f=(install_fixture){.fail=phase};rf=(resource_fixture){.server=&s};
        assert(!installing(&s,&f,&rf,&report) && session.result.error==TV4_SESSION_STORAGE && f.abort==1 && !f.selected);
    }
    fixture();s=healthy();f=(install_fixture){.cancel_seal=true};rf=(resource_fixture){.server=&s};
    assert(!installing(&s,&f,&rf,&report) && session.result.state==TV4_SESSION_CANCELLED && f.seal==1 && !f.activate && f.abort==1);
    fixture();s=healthy();binary[479]^=1;f=(install_fixture){0};rf=(resource_fixture){.server=&s};
    assert(!installing(&s,&f,&rf,&report) && f.used>0 && f.used<=480 && !f.seal && !f.activate && f.abort==1);
    fixture();s=healthy();manifest[0]='[';f=(install_fixture){0};rf=(resource_fixture){.server=&s};
    assert(!installing(&s,&f,&rf,&report) && !f.begin && !f.write && !f.activate);
    fixture();s=healthy();f=(install_fixture){0};rf=(resource_fixture){.server=&s};
    assert(installing(&s,&f,&rf,&report));checks=report.checks;
    for(unsigned boundary=1;boundary<=checks;boundary++) {
        tv4_ota_session_reset(&session);s=healthy();f=(install_fixture){0};rf=(resource_fixture){.server=&s,.fail_at=boundary};
        assert(!installing(&s,&f,&rf,&report) && !f.selected && !f.activate && f.abort==1);
    }
    for(unsigned call=1;call<=3;call++) {
        fixture();s=healthy();s.fail_call=call;f=(install_fixture){0};rf=(resource_fixture){.server=&s};
        assert(!installing(&s,&f,&rf,&report) && !f.selected && !f.activate);
    }
    for(unsigned call=1;call<=3;call++) {
        fixture();s=healthy();s.cancel_call=call;f=(install_fixture){0};rf=(resource_fixture){.server=&s};
        assert(!installing(&s,&f,&rf,&report) && !f.selected && !f.activate && session.result.state==TV4_SESSION_CANCELLED);
    }
    for(size_t cut=0;cut<sizeof(binary);cut+=17) {
        fixture();s=healthy();s.cut=cut;f=(install_fixture){0};rf=(resource_fixture){.server=&s};
        assert(!installing(&s,&f,&rf,&report) && !f.selected && !f.activate);
    }
    /* Interrupt every partial transport chunk after a real sink write. Neither
     * operator cancellation nor lost transport may seal/select that image. */
    for(unsigned disconnected=0;disconnected<2;disconnected++)for(size_t cut=17;cut<sizeof(binary);cut+=17) {
        fixture();s=healthy();f=(install_fixture){0};rf=(resource_fixture){.server=&s};
        if(disconnected)s.disconnect_after=cut;else f.cancel_after=cut;
        assert(!installing(&s,&f,&rf,&report));
        assert(f.used==cut && f.used>0 && f.used<sizeof(binary));
        assert(!memcmp(f.bytes,binary,cut) && f.begin==1 && f.abort==1);
        assert(!f.seal && !f.activate && !f.selected && s.opens==3 && s.closes==3);
        assert(session.result.image_bytes==cut && report.result==TV4_RESOURCE_OK);
        assert(session.result.state==(disconnected?TV4_SESSION_FAILED:TV4_SESSION_CANCELLED));
        assert(session.result.error==(disconnected?TV4_SESSION_TRANSPORT:TV4_SESSION_CANCEL));
        assert(!session.manifest_length && !session.envelope_length && !session.candidate.size_bytes);
        /* A fresh operation in the same process must authenticate again and
         * replace the partial sink contents without resurrecting failed state. */
        tv4_ota_session_reset(&session);server retry=healthy();retry.now=s.now;
        install_fixture again={0};memcpy(again.bytes,f.bytes,sizeof(again.bytes));
        resource_fixture retry_resources={.server=&retry};
        assert(installing(&retry,&again,&retry_resources,&report));
        assert(session.result.state==TV4_SESSION_INSTALLED && session.result.image_bytes==sizeof(binary));
        assert(again.begin==1 && again.seal==1 && again.activate==1 && !again.abort && again.selected);
        assert(again.used==sizeof(binary) && !memcmp(again.bytes,binary,sizeof(binary)));
    }
    printf("Installer actual signed session: positive + begin/write/seal/activate failures, cancel after seal, digest/signature/transport and %u resource boundaries; no premature boot selection PASS\n",checks);
}
int main(void) {
    key=EVP_PKEY_Q_keygen(NULL,NULL,"EC","prime256v1");assert(key);
    int public_length=i2d_PUBKEY(key,&public_key);assert(public_length==91);
    trust=(tv4_ota_context){"test-only","0.1.0","0.1.0","1.0.0",NULL,"tv4-16m-ab-v1",false,true,public_key,(size_t)public_length};
    installation_regressions();
    resource_regressions();
    fixture();server s=healthy();assert(run(&s,request()));
    assert(session.result.state==TV4_SESSION_IMAGE_VERIFIED && session.result.image_bytes==sizeof(binary));
    assert(s.calls==3 && s.stages[0]==TV4_SESSION_MANIFEST && s.stages[1]==TV4_SESSION_ENVELOPE && s.stages[2]==TV4_SESSION_IMAGE);
    assert(!tv4_ota_session_run(&session,NULL,NULL));assert(session.result.state==TV4_SESSION_IMAGE_VERIFIED);
    fixture();s=healthy();tv4_session_request r=request();r.verify_image=false;
    assert(run(&s,r) && s.calls==2 && session.result.state==TV4_SESSION_CANDIDATE_READY);
    for(unsigned call=1;call<=3;call++) {
        fixture();s=healthy();s.fail_call=call;assert(!run(&s,request()));
        assert(s.calls==call && session.result.error==TV4_SESSION_TRANSPORT);
        fixture();s=healthy();s.cancel_call=call;assert(!run(&s,request()));
        assert(s.calls==call && session.result.state==TV4_SESSION_CANCELLED);
    }
    fixture();s=healthy();s.cancel=true;assert(!run(&s,request()) && !s.calls);
    fixture();s=healthy();manifest[0]='[';assert(!run(&s,request()) && s.calls==2);
    assert(session.result.policy==TV4_OTA_BAD_SIGNATURE && session.result.failed_stage==TV4_SESSION_AUTHENTICATING);
    fixture();s=healthy();envelope[0]='[';assert(!run(&s,request()) && s.calls==2 && session.result.policy==TV4_OTA_BAD_ENVELOPE);
    fixture();s=healthy();binary[479]^=1;assert(!run(&s,request()));
    assert(session.result.error==TV4_SESSION_FORMAT && session.result.image==TV4_IMAGE_DIGEST);
    fixture();s=healthy();binary[0]=0;assert(!run(&s,request()));
    assert(session.result.error==TV4_SESSION_FORMAT && session.result.image==TV4_IMAGE_HEADER);
    for(size_t cut=0;cut<sizeof(binary);cut+=17) {
        fixture();s=healthy();s.cut=cut;assert(!run(&s,request()));assert(session.result.error==TV4_SESSION_TRANSPORT);
    }
    for(unsigned call=1;call<=2;call++) {
        fixture();s=healthy();s.consumer_call=call;assert(!run(&s,request()));assert(s.calls==call);
    }
    fixture();s=healthy();s.expire_auth=true;assert(!run(&s,request()) && s.calls==2);
    assert(session.result.error==TV4_SESSION_TIMEOUT && session.result.failed_stage==TV4_SESSION_AUTHENTICATING);
    fixture();s=healthy();s.cancel_auth=true;assert(!run(&s,request()) && s.calls==2);
    assert(session.result.state==TV4_SESSION_CANCELLED && session.result.failed_stage==TV4_SESSION_AUTHENTICATING);
    fixture();s=healthy();s.now=UINT32_MAX-20;s.read_delay=1;assert(run(&s,request()));
    fixture();s=healthy();s.read_delay=10;r=request();r.idle_timeout_ms=200;r.total_timeout_ms=(uint32_t)(((manifest_length+16)/17+1)*10+30);
    assert(!run(&s,r) && s.calls==2 && session.result.error==TV4_SESSION_TIMEOUT);
    fixture();s=healthy();s.read_delay=10;r=request();r.image_limits.idle_timeout_ms=9;
    assert(!run(&s,r) && session.result.error==TV4_SESSION_TIMEOUT && session.result.image==TV4_IMAGE_TIMEOUT);
    fixture();s=healthy();r=request();r.manifest_url="https://github.com/avlyubimov/TV4-CNC/releases/download/v0.3.0/manifest.json";
    assert(!run(&s,r) && s.calls==2 && session.result.policy==TV4_OTA_WRONG_VERSION);
    fixture();s=healthy();r=request();r.trusted_time=false;assert(!run(&s,r) && s.calls==1 && session.result.http==TV4_HTTP_UNTRUSTED_TIME);
    fixture();s=healthy();r=request();r.manifest_url="http://github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0/manifest.json";
    assert(!run(&s,r) && !s.calls && session.result.error==TV4_SESSION_ARGUMENT);
    fixture();s=healthy();r=request();r.image_limits.chip_revision_known=false;
    assert(!run(&s,r) && s.calls==2 && session.result.error==TV4_SESSION_FORMAT);
    tv4_ota_session_reset(&session);assert(session.result.state==TV4_SESSION_IDLE);
    OPENSSL_free(public_key);EVP_PKEY_free(key);
    printf("Read-only session signed end-to-end/failure/cancel/truncation/deadline/reset PASS; workspace=%zu bytes (host)\n",sizeof(session));
}
