#include "release_admission.h"
#include "release_metadata.h"
#include <assert.h>
#include <inttypes.h>
#include <mbedtls/platform.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <stdio.h>
#include <string.h>

#define TEST_COMMIT "0123456789abcdef0123456789abcdef01234567"
typedef struct {
    tv4_boot_release_policy policy;
    tv4_boot_request request;
    tv4_boot_workspace workspace;
    tv4_boot_release_candidate candidate;
    tv4_boot_key key;
    EVP_PKEY *signer;
    uint8_t metadata[1024], signature[72], image[1024];
    size_t source_bytes, fragment;
    uint32_t now, cost;
    unsigned reads, clocks, fault;
    bool cancel, cancel_eof, cancel_cleanup, mutate, reenter;
} fixture;
static bool zero(const void *pointer, size_t size)
{
    const uint8_t *p = pointer;
    for (size_t i = 0; i < size; ++i) if (p[i] != 0) return false;
    return true;
}
static void sha(const void *bytes, size_t count, uint8_t out[32])
{
    size_t length = 0;
    assert(EVP_Q_digest(NULL, "SHA256", NULL, bytes, count, out, &length) == 1 && length == 32);
}
static void sign_document(fixture *f)
{
    EVP_MD_CTX *c = EVP_MD_CTX_new(); assert(c != NULL);
    assert(EVP_DigestSignInit(c, NULL, EVP_sha256(), NULL, f->signer) == 1);
    size_t n = sizeof(f->signature);
    assert(EVP_DigestSign(c, f->signature, &n, f->metadata, f->request.metadata_bytes) == 1);
    f->request.signature_bytes = n;
    EVP_MD_CTX_free(c);
}
static void compose(fixture *f, const char *release, const char *component, const char *channel)
{
    uint8_t digest[32]; char hex[65]; sha(f->image, 769, digest);
    for (size_t i = 0; i < 32; ++i) (void)snprintf(hex + i * 2, 3, "%02x", digest[i]);
    int n = snprintf((char *)f->metadata, sizeof(f->metadata),
        "{\"schema\":\"%s\",\"component\":\"motion\",\"hardware_id\":\"%s\","
        "\"hardware_revision\":\"%s\",\"memory_profile\":\"%s\","
        "\"image_counter\":3,\"minimum_bootloader_counter\":%" PRIu32 ","
        "\"protocol_min\":%" PRIu32 ",\"protocol_max\":%" PRIu32 ","
        "\"image_size\":769,\"sha256\":\"%s\",\"release_version\":\"%s\","
        "\"component_version\":\"%s\",\"channel\":\"%s\",\"source_commit\":\"%s\"}",
        TV4_BOOT_RELEASE_SCHEMA, f->policy.image.hardware_id, f->policy.image.hardware_revision,
        f->policy.image.memory_profile, f->policy.image.bootloader_counter,
        f->policy.image.protocol, f->policy.image.protocol, hex, release, component, channel, TEST_COMMIT);
    assert(n > 0 && (size_t)n < sizeof(f->metadata));
    f->request.metadata_bytes = (size_t)n; sign_document(f);
}
static void edit(fixture *f, const char *from, const char *to)
{
    char *p = strstr((char *)f->metadata, from); assert(p != NULL);
    size_t start = (size_t)(p - (char *)f->metadata), old = strlen(from), n = strlen(to);
    assert(f->request.metadata_bytes - old + n < sizeof(f->metadata));
    memmove(p + n, p + old, f->request.metadata_bytes - start - old + 1);
    memcpy(p, to, n); f->request.metadata_bytes = f->request.metadata_bytes - old + n;
    sign_document(f);
}
static uint32_t now_ms(void *context)
{
    fixture *f = context;
    if (++f->clocks == 1 && f->mutate) {
        memset(f->metadata, 0xee, sizeof(f->metadata)); memset(f->signature, 0xee, sizeof(f->signature));
        memset(&f->key, 0, sizeof(f->key)); memset(&f->policy, 0, sizeof(f->policy));
        f->request.read = NULL; f->request.now_ms = NULL;
    }
    return f->now;
}
static bool cancelled(void *context)
{
    fixture *f = context;
    return f->cancel || (f->cancel_cleanup && f->clocks > 2 && zero(&f->workspace, sizeof(f->workspace)));
}
static bool read_memory(void *context, uint32_t offset, uint8_t *bytes, size_t capacity,
    size_t *count, bool *eof)
{
    fixture *f = context; ++f->reads; f->now += f->cost;
    if (f->reenter) {
        tv4_boot_release_candidate other, before; memset(&other, 0xa5, sizeof(other)); before = other;
        assert(tv4_boot_admit_release(&f->policy, &f->request, &f->workspace, &other) == TV4_BOOT_BUSY);
        assert(memcmp(&other, &before, sizeof(other)) == 0);
        tv4_boot_candidate legacy, saved; memset(&legacy, 0x5a, sizeof(legacy)); saved = legacy;
        assert(tv4_boot_admit(&f->policy.image, &f->request, &f->workspace, &legacy) == TV4_BOOT_BUSY);
        assert(memcmp(&legacy, &saved, sizeof(legacy)) == 0);
    }
    if (f->fault == 1) return false;
    if (f->fault == 2) { *count = capacity + 1; return true; }
    if (f->fault == 3) { *count = 0; *eof = false; return true; }
    if (f->fault == 4) { *count = 0; *eof = true; return true; }
    if (f->fault == 5) f->now = 999;
    assert(offset <= f->source_bytes);
    size_t n = f->source_bytes - offset;
    if (n > capacity) n = capacity;
    if (n > f->fragment) n = f->fragment;
    memcpy(bytes, f->image + offset, n); *count = n; *eof = offset + n == f->source_bytes;
    if (f->cancel_eof && *eof && n == 0) f->cancel = true;
    return true;
}
static void setup(fixture *f)
{
    *f = (fixture){0};
    f->signer = EVP_PKEY_Q_keygen(NULL, NULL, "EC", "prime256v1"); assert(f->signer != NULL);
    memcpy(f->key.id, "test-only", 10); unsigned char *der = f->key.spki;
    assert(i2d_PUBKEY(f->signer, &der) == TV4_BOOT_KEY_BYTES);
    f->policy = (tv4_boot_release_policy){
        .image = {.hardware_id="test-f407", .hardware_revision="synthetic", .memory_profile="test-no-addresses",
            .installed_image_counter=2, .minimum_image_counter=3, .bootloader_counter=5, .protocol=7,
            .max_image_bytes=1024, .timeout_ms=5000, .keys=&f->key, .key_count=1},
        .installed_component_version="2.0.0", .minimum_component_version="2.0.1",
        .selected_release_version="3.0.0", .selected_source_commit=TEST_COMMIT, .channel="stable"};
    f->request = (tv4_boot_request){.context=f, .now_ms=now_ms, .cancelled=cancelled, .read=read_memory,
        .metadata=f->metadata, .signature=f->signature, .key_id=(const uint8_t *)"test-only", .key_id_bytes=9,
        .arena_bytes=TV4_BOOT_ARENA_BYTES};
    f->source_bytes=769; f->fragment=256; f->now=1000;
    for (size_t i=0; i<sizeof(f->image); ++i) f->image[i]=(uint8_t)(i*37u);
    compose(f,"3.0.0","2.1.0","stable");
    memset(&f->workspace,0xa5,sizeof(f->workspace)); memset(&f->candidate,0xa5,sizeof(f->candidate));
}
static void done(fixture *f) { EVP_PKEY_free(f->signer); }
static tv4_boot_result run(fixture *f)
{
    tv4_boot_result r=tv4_boot_admit_release(&f->policy,&f->request,&f->workspace,&f->candidate);
    if (r!=TV4_BOOT_VERIFIED_CANDIDATE) assert(zero(&f->candidate,sizeof(f->candidate)));
    if (f->clocks!=0) assert(zero(&f->workspace,sizeof(f->workspace)));
    assert(mbedtls_calloc(1,8)==NULL);
    return r;
}
static void success(void)
{
    const size_t fragments[]={1,7,255,256};
    for (size_t i=0;i<4;++i) {
        fixture f; setup(&f); f.fragment=fragments[i];
        uint8_t mh[32], ih[32]; sha(f.metadata,f.request.metadata_bytes,mh); sha(f.image,769,ih);
        assert(run(&f)==TV4_BOOT_VERIFIED_CANDIDATE);
        assert(f.candidate.image.image_bytes==769 && f.candidate.image.image_counter==3);
        assert(!memcmp(f.candidate.image.image_sha256,ih,32) && !memcmp(f.candidate.image.metadata_sha256,mh,32));
        assert(!strcmp(f.candidate.release_version,"3.0.0") && !strcmp(f.candidate.component_version,"2.1.0"));
        assert(!strcmp(f.candidate.source_commit,TEST_COMMIT) && !strcmp(f.candidate.image.key_id,"test-only"));
        assert(f.reads==(769u+fragments[i]-1u)/fragments[i]+1u); done(&f);
    }
    fixture f; setup(&f); strcpy(f.policy.selected_release_version,"4.0.0-rc.1"); strcpy(f.policy.channel,"prerelease");
    compose(&f,"4.0.0-rc.1","2.1.0","prerelease"); assert(run(&f)==TV4_BOOT_VERIFIED_CANDIDATE); done(&f);
    setup(&f); strcpy(f.policy.selected_release_version,"4.0.0-rc.2"); strcpy(f.policy.channel,"prerelease");
    compose(&f,"4.0.0-rc.2","2.1.0-rc.1","prerelease"); assert(run(&f)==TV4_BOOT_VERIFIED_CANDIDATE); done(&f);
}
static void signed_selection_and_rollback(void)
{
    struct change { const char *from,*to; tv4_boot_result expected; } cases[]={
        {"\"release_version\":\"3.0.0\"","\"release_version\":\"3.0.1\"",TV4_BOOT_COMPATIBILITY},
        {TEST_COMMIT,"abcdef0123456789abcdef0123456789abcdef01",TV4_BOOT_COMPATIBILITY},
        {"\"channel\":\"stable\"","\"channel\":\"prerelease\"",TV4_BOOT_COMPATIBILITY},
        {"\"component_version\":\"2.1.0\"","\"component_version\":\"2.0.0\"",TV4_BOOT_ROLLBACK},
        {"\"component_version\":\"2.1.0\"","\"component_version\":\"1.9.9\"",TV4_BOOT_ROLLBACK},
        {"\"component_version\":\"2.1.0\"","\"component_version\":\"2.1.0-rc.1\"",TV4_BOOT_COMPATIBILITY},
        {"\"image_counter\":3","\"image_counter\":2",TV4_BOOT_ROLLBACK},
        {"test-f407","different-board",TV4_BOOT_COMPATIBILITY},
        {"test-no-addresses","other-model",TV4_BOOT_COMPATIBILITY},
        {"\"protocol_max\":7","\"protocol_max\":6",TV4_BOOT_COMPATIBILITY},
        {"\"minimum_bootloader_counter\":5","\"minimum_bootloader_counter\":6",TV4_BOOT_COMPATIBILITY}
    };
    for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        fixture f; setup(&f); edit(&f,cases[i].from,cases[i].to);
        tv4_boot_result got=run(&f);
        if(got!=cases[i].expected) fprintf(stderr,"signed selection case %zu: got=%d expected=%d\n",i,got,cases[i].expected);
        assert(got==cases[i].expected);
        assert(f.reads==0); done(&f);
    }
    fixture f; setup(&f); strcpy(f.policy.minimum_component_version,"2.2.0");
    assert(run(&f)==TV4_BOOT_ROLLBACK && f.reads==0); done(&f);
    setup(&f); f.policy.image.minimum_image_counter=4;
    assert(run(&f)==TV4_BOOT_ROLLBACK && f.reads==0); done(&f);
}
static void strict_schema(void)
{
    const char *from[]={"tv4-motion-release-draft-1","\"component\":\"motion\"","\"image_counter\":3", "\"image_counter\":3",
        "\"release_version\":\"3.0.0\"","\"component_version\":\"2.1.0\"","\"channel\":\"stable\"",TEST_COMMIT,TEST_COMMIT,
        "\"image_counter\":3","\"image_counter\":3","\"image_counter\":3","\"image_counter\":3","\"image_counter\":3"};
    const char *to[]={TV4_BOOT_DRAFT_SCHEMA,"\"component\":\"hmi\"","\"image_counter\":03","\"image_counter\":3e0",
        "\"release_version\":\"3.0\"","\"component_version\":\"2.1.0+build\"","\"channel\":\"unknown\"",
        "0000000000000000000000000000000000000000","ABCDEF0123456789abcdef0123456789abcdef01",
        "\"image_counter\":3,\"image_counter\":3","\"unknown\":3","\"image_counter\":[]","\"image_counter\":-1","\"image_counter\":4294967296"};
    for (size_t i=0;i<sizeof(from)/sizeof(from[0]);++i) {
        fixture f; setup(&f); edit(&f,from[i],to[i]); assert(run(&f)==TV4_BOOT_METADATA && f.reads==0); done(&f);
    }
    fixture f; setup(&f); edit(&f,"\"channel\":\"stable\",","");
    assert(run(&f)==TV4_BOOT_METADATA); done(&f);
    setup(&f); f.metadata[f.request.metadata_bytes++]='x'; sign_document(&f);
    assert(run(&f)==TV4_BOOT_METADATA); done(&f);
    setup(&f); f.metadata[10]=0; sign_document(&f);
    assert(run(&f)==TV4_BOOT_METADATA); done(&f);
    setup(&f); --f.request.metadata_bytes; sign_document(&f);
    assert(run(&f)==TV4_BOOT_METADATA); done(&f);
    /* Raw whitespace is signed; order need not match the serializer. */
    setup(&f); edit(&f,"{\"schema\"","{\n \"schema\"");
    edit(&f,"\"image_counter\":3,\"minimum_bootloader_counter\":5","\"minimum_bootloader_counter\":5,\"image_counter\":3");
    assert(run(&f)==TV4_BOOT_VERIFIED_CANDIDATE); done(&f);
}
static void unknown_context(void)
{
    for (unsigned i=0;i<9;++i) {
        fixture f; setup(&f);
        switch(i) {
        case 0: f.policy.installed_component_version[0]=0; break;
        case 1: strcpy(f.policy.installed_component_version,"git-a91b1fa"); break;
        case 2: f.policy.minimum_component_version[0]=0; break;
        case 3: f.policy.selected_release_version[0]=0; break;
        case 4: f.policy.selected_source_commit[0]=0; break;
        case 5: strcpy(f.policy.channel,"unknown"); break;
        case 6: strcpy(f.policy.selected_release_version,"3.0.0-rc.1"); break;
        case 7: memset(f.policy.installed_component_version,'a',32); break;
        default: f.policy.image.protocol=0; break;
        }
        assert(run(&f)==TV4_BOOT_POLICY && f.reads==0 && f.clocks==0); done(&f);
    }
}
static void crypto_and_stream(void)
{
    fixture f; setup(&f); f.metadata[0]=' '; assert(run(&f)==TV4_BOOT_SIGNATURE && f.reads==0); done(&f);
    setup(&f); f.signature[f.request.signature_bytes-1]^=1; assert(run(&f)==TV4_BOOT_SIGNATURE); done(&f);
    setup(&f); f.request.key_id=(const uint8_t *)"other-key"; assert(run(&f)==TV4_BOOT_KEY); done(&f);
    setup(&f); f.key.spki[0]^=1; assert(run(&f)==TV4_BOOT_KEY); done(&f);
    setup(&f); f.image[500]^=1; assert(run(&f)==TV4_BOOT_HASH); done(&f);
    setup(&f); f.source_bytes=768; assert(run(&f)==TV4_BOOT_LENGTH); done(&f);
    setup(&f); f.source_bytes=770; assert(run(&f)==TV4_BOOT_LENGTH); done(&f);
    const tv4_boot_result expected[]={TV4_BOOT_SOURCE,TV4_BOOT_SOURCE,TV4_BOOT_SOURCE,TV4_BOOT_LENGTH,TV4_BOOT_CLOCK};
    for (unsigned i=1;i<=5;++i) { setup(&f); f.fault=i; assert(run(&f)==expected[i-1]); done(&f); }
    setup(&f); f.request.arena_bytes=1024; assert(run(&f)==TV4_BOOT_RESOURCE); done(&f);
    setup(&f); f.cost=5000; assert(run(&f)==TV4_BOOT_TIMEOUT); done(&f);
    setup(&f); f.now=UINT32_MAX-1u; f.cost=1; assert(run(&f)==TV4_BOOT_VERIFIED_CANDIDATE); done(&f);
    setup(&f); f.cancel=true; assert(run(&f)==TV4_BOOT_CANCELLED); done(&f);
    setup(&f); f.cancel_eof=true; assert(run(&f)==TV4_BOOT_CANCELLED); done(&f);
    setup(&f); f.cancel_cleanup=true; assert(run(&f)==TV4_BOOT_CANCELLED); done(&f);
}
static void snapshot_reentry_and_domain(void)
{
    fixture f; setup(&f); f.mutate=true; assert(run(&f)==TV4_BOOT_VERIFIED_CANDIDATE);
    assert(!strcmp(f.candidate.release_version,"3.0.0") && !strcmp(f.candidate.source_commit,TEST_COMMIT)); done(&f);
    setup(&f); f.reenter=true; assert(run(&f)==TV4_BOOT_VERIFIED_CANDIDATE); done(&f);
    setup(&f); tv4_boot_candidate old;
    assert(tv4_boot_admit(&f.policy.image,&f.request,&f.workspace,&old)==TV4_BOOT_METADATA && f.reads==0);
    assert(zero(&old,sizeof(old))); done(&f);
    setup(&f); edit(&f,TV4_BOOT_RELEASE_SCHEMA,TV4_BOOT_DRAFT_SCHEMA);
    char *extra=strstr((char *)f.metadata,",\"release_version\""); assert(extra!=NULL);
    *extra++='}'; *extra=0; f.request.metadata_bytes=(size_t)(extra-(char *)f.metadata); sign_document(&f);
    assert(run(&f)==TV4_BOOT_METADATA && f.reads==0);
    f.reenter=true; assert(tv4_boot_admit(&f.policy.image,&f.request,&f.workspace,&old)==TV4_BOOT_VERIFIED_CANDIDATE);
    done(&f);
}
static void aliases_and_arguments(void)
{
    fixture f; setup(&f); tv4_boot_release_policy before=f.policy;
    assert(tv4_boot_admit_release(&f.policy,&f.request,&f.workspace,(tv4_boot_release_candidate *)(void *)&f.policy)==TV4_BOOT_ARGUMENT);
    assert(!memcmp(&before,&f.policy,sizeof(before)));
    tv4_boot_workspace saved=f.workspace;
    assert(tv4_boot_admit_release(&f.policy,&f.request,&f.workspace,(tv4_boot_release_candidate *)(void *)&f.workspace)==TV4_BOOT_ARGUMENT);
    assert(!memcmp(&saved,&f.workspace,sizeof(saved)));
    uint8_t metadata_before[1024]; memcpy(metadata_before,f.metadata,1024);
    assert(tv4_boot_admit_release(&f.policy,&f.request,&f.workspace,(tv4_boot_release_candidate *)(void *)f.metadata)==TV4_BOOT_ARGUMENT);
    assert(!memcmp(metadata_before,f.metadata,1024));
    assert(tv4_boot_admit_release(NULL,&f.request,&f.workspace,&f.candidate)==TV4_BOOT_ARGUMENT && zero(&f.candidate,sizeof(f.candidate)));
    assert(tv4_boot_admit_release(&f.policy,NULL,&f.workspace,&f.candidate)==TV4_BOOT_ARGUMENT);
    assert(tv4_boot_admit_release(&f.policy,&f.request,NULL,&f.candidate)==TV4_BOOT_ARGUMENT);
    assert(tv4_boot_admit_release(&f.policy,&f.request,&f.workspace,NULL)==TV4_BOOT_ARGUMENT);
    f.request.metadata_bytes=1025; assert(run(&f)==TV4_BOOT_ARGUMENT); done(&f);
}
static void bounds(void)
{
    fixture f; setup(&f);
    memset(f.policy.image.hardware_id,'a',31); f.policy.image.hardware_id[31]=0;
    memset(f.policy.image.hardware_revision,'b',31); f.policy.image.hardware_revision[31]=0;
    memset(f.policy.image.memory_profile,'c',31); f.policy.image.memory_profile[31]=0;
    strcpy(f.policy.selected_release_version,"1.0.0-9999999999999999999999999");
    strcpy(f.policy.channel,"prerelease");
    f.policy.image.bootloader_counter=UINT32_MAX; f.policy.image.protocol=UINT32_MAX;
    compose(&f,f.policy.selected_release_version,"2.1.0-9999999999999999999999999","prerelease");
    size_t n=f.request.metadata_bytes;
    assert(n<1024); memset(f.metadata+n,' ',1024-n); f.request.metadata_bytes=1024; sign_document(&f);
    assert(run(&f)==TV4_BOOT_VERIFIED_CANDIDATE);
    printf("PASS maximum-width fields: document=%zu padded=1024 workspace=%zu candidate=%zu policy=%zu\n",n,sizeof(f.workspace),sizeof(f.candidate),sizeof(f.policy));
    done(&f);
}
static char *decode(char *hex, char out[96])
{
    if (!strcmp(hex,"-")) return NULL;
    size_t n=strlen(hex); assert(n%2==0 && n/2<96);
    for(size_t i=0;i<n;i+=2) {
        unsigned v=0;
        for(size_t j=0;j<2;++j) { char c=hex[i+j]; assert((c>='0'&&c<='9')||(c>='a'&&c<='f')); v=v*16u+(unsigned)(c<='9'?c-'0':c-'a'+10); }
        out[i/2]=(char)v;
    }
    out[n/2]=0; return out;
}
static void version_driver(void)
{
    char line[400];
    while(fgets(line,sizeof(line),stdin)) {
        char *separator=strchr(line,' '); assert(separator!=NULL); *separator++=0;
        char *end=strchr(separator,'\n'); assert(end!=NULL); *end=0;
        char a[96],b[96]; int order=55;
        bool ok=tv4_boot_compare_versions(decode(line,a),decode(separator,b),&order);
        if(!ok) assert(order==55);
        printf("%d\n",ok?order:9);
    }
}
int main(int argc, char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--versions")) { version_driver(); return 0; }
    assert(argc==1);
    success(); signed_selection_and_rollback(); strict_schema(); unknown_context();
    crypto_and_stream(); snapshot_reentry_and_domain(); aliases_and_arguments(); bounds();
    puts("PASS 8 signed release admission groups; raw P-256/SHA, versions/selection/target/image, cleanup/faults; RAM only, no install/device access");
    return 0;
}
