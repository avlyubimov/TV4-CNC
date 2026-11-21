/* HOST ONLY: explicit local inputs, independent verifier, historical output.
 * Not firmware, signing service, live lease/model check or installation gate. */
#include "release_admission.h"
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

enum { HOST_IMAGE_MAX=16*1024*1024, HOST_TIMEOUT_MAX=10000 };
typedef struct {FILE *file;uint32_t seen;bool clock_failed;} source;
static bool same_file(const struct stat *a,const struct stat *b)
{
    return S_ISREG(b->st_mode) && a->st_dev==b->st_dev && a->st_ino==b->st_ino && a->st_size==b->st_size &&
        a->st_mtim.tv_sec==b->st_mtim.tv_sec && a->st_mtim.tv_nsec==b->st_mtim.tv_nsec &&
        a->st_ctim.tv_sec==b->st_ctim.tv_sec && a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
}
static FILE *regular(const char *path,size_t limit,struct stat *saved)
{
    struct stat before,after;
    if(lstat(path,&before)!=0 || !S_ISREG(before.st_mode) || before.st_size<=0 ||
        (uintmax_t)before.st_size>limit) return NULL;
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0) return NULL;
    if(fstat(fd,&after)!=0 || !same_file(&before,&after)) {(void)close(fd);return NULL;}
    FILE *f=fdopen(fd,"rb");
    if(f==NULL) {(void)close(fd);return NULL;}
    *saved=after;return f;
}
static bool small(const char *path,uint8_t *bytes,size_t maximum,size_t *length)
{
    struct stat saved,after;FILE *f=regular(path,maximum,&saved);
    if(f==NULL) return false;
    size_t n=fread(bytes,1,maximum+1u,f);
    bool ok=n>0 && n<=maximum && !ferror(f) && fstat(fileno(f),&after)==0 && same_file(&saved,&after);
    if(fclose(f)!=0) ok=false;
    *length=n;return ok;
}
static bool text(char *out,size_t capacity,const char *arg)
{
    size_t n=strlen(arg);
    if(n==0 || n>=capacity) return false;
    memcpy(out,arg,n+1u);return true;
}
static bool number(const char *arg,uint32_t *out)
{
    if(arg[0]<'1' || arg[0]>'9') return false;
    uint32_t n=0;
    for(size_t i=0;arg[i]!=0;++i) {
        if(arg[i]<'0' || arg[i]>'9') return false;
        uint32_t digit=(uint32_t)(arg[i]-'0');
        if(n>(UINT32_MAX-digit)/10u) return false;
        n=n*10u+digit;
    }
    *out=n;return true;
}
static uint32_t now_ms(void *context)
{
    source *s=context;struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t)!=0 || t.tv_sec<0 || t.tv_nsec<0) {s->clock_failed=true;return 0;}
    return (uint32_t)((uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u);
}
static bool cancelled(void *context) {return ((source *)context)->clock_failed;}
static bool read_image(void *context,uint32_t offset,uint8_t *bytes,size_t capacity,size_t *count,bool *eof)
{
    source *s=context;
    if(offset!=s->seen || capacity==0 || capacity>TV4_BOOT_CHUNK_BYTES) return false;
    size_t n=fread(bytes,1,capacity,s->file);
    if(ferror(s->file)) return false;
    s->seen+=(uint32_t)n;*count=n;*eof=feof(s->file)!=0;return true;
}
static bool zero(const void *data,size_t n)
{
    const uint8_t *p=data;
    for(size_t i=0;i<n;++i)if(p[i]!=0)return false;
    return true;
}
static void hex(const uint8_t hash[32]) {for(size_t i=0;i<32;++i)(void)printf("%02x",hash[i]);}
int main(int argc,char **argv)
{
    /* Paths meta/sig/public/image; keyid/hw/rev/profile; counters current/min/
     * boot/protocol; max bytes/timeout; installed/minimum/release/source/channel.
     * Every value is a mandatory LOCAL input, never inferred from metadata. */
    if(argc!=20) {fputs("Motion review: expected 19 explicit arguments; see RELEASE_COHORT_REVIEW.md\n",stderr);return 2;}
    tv4_boot_key key={0};tv4_boot_release_policy p={0};
    if(!text(key.id,sizeof(key.id),argv[5]) || !text(p.image.hardware_id,sizeof(p.image.hardware_id),argv[6]) ||
        !text(p.image.hardware_revision,sizeof(p.image.hardware_revision),argv[7]) ||
        !text(p.image.memory_profile,sizeof(p.image.memory_profile),argv[8]) ||
        !number(argv[9],&p.image.installed_image_counter) || !number(argv[10],&p.image.minimum_image_counter) ||
        !number(argv[11],&p.image.bootloader_counter) || !number(argv[12],&p.image.protocol) ||
        !number(argv[13],&p.image.max_image_bytes) || !number(argv[14],&p.image.timeout_ms) ||
        p.image.max_image_bytes>HOST_IMAGE_MAX || p.image.timeout_ms>HOST_TIMEOUT_MAX ||
        !text(p.installed_component_version,sizeof(p.installed_component_version),argv[15]) ||
        !text(p.minimum_component_version,sizeof(p.minimum_component_version),argv[16]) ||
        !text(p.selected_release_version,sizeof(p.selected_release_version),argv[17]) ||
        !text(p.selected_source_commit,sizeof(p.selected_source_commit),argv[18]) || !text(p.channel,sizeof(p.channel),argv[19])) {
        fputs("Motion review: invalid or unbounded local context\n",stderr);return 2;
    }
    uint8_t metadata[TV4_BOOT_METADATA_MAX+1u],signature[TV4_BOOT_SIGNATURE_MAX+1u],public_key[TV4_BOOT_KEY_BYTES+1u];
    size_t mn=0,sn=0,kn=0;
    if(!small(argv[1],metadata,TV4_BOOT_METADATA_MAX,&mn) || !small(argv[2],signature,TV4_BOOT_SIGNATURE_MAX,&sn) ||
        !small(argv[3],public_key,TV4_BOOT_KEY_BYTES,&kn) || kn!=TV4_BOOT_KEY_BYTES) {
        fputs("Motion review: bounded regular metadata/signature/public key required\n",stderr);return 2;
    }
    memcpy(key.spki,public_key,kn);p.image.keys=&key;p.image.key_count=1;
    struct stat saved,after;source s={.file=regular(argv[4],p.image.max_image_bytes,&saved)};
    if(s.file==NULL) {fputs("Motion review: bounded regular image required\n",stderr);return 2;}
    (void)now_ms(&s);
    if(s.clock_failed) {(void)fclose(s.file);fputs("Motion review: monotonic clock unavailable\n",stderr);return 2;}
    tv4_boot_request r={.context=&s,.now_ms=now_ms,.cancelled=cancelled,.read=read_image,
        .metadata=metadata,.metadata_bytes=mn,.signature=signature,.signature_bytes=sn,
        .key_id=(const uint8_t *)key.id,.key_id_bytes=strlen(key.id),.arena_bytes=TV4_BOOT_ARENA_BYTES};
    tv4_boot_workspace w={0};tv4_boot_release_candidate candidate={0};
    tv4_boot_result result=tv4_boot_admit_release(&p,&r,&w,&candidate);
    bool unchanged=fstat(fileno(s.file),&after)==0 && same_file(&saved,&after);
    if(fclose(s.file)!=0) unchanged=false;
    if(!unchanged) {result=TV4_BOOT_SOURCE;candidate=(tv4_boot_release_candidate){0};}
    bool wiped=zero(&w,sizeof(w));
    if(result!=TV4_BOOT_VERIFIED_CANDIDATE || !wiped) {
        (void)printf("{\"kind\":\"tv4-motion-host-review-1\",\"status\":\"rejected\",\"result\":%u,\"candidate_zero\":%s,\"workspace_zero\":%s,\"device_access\":false,\"install_enabled\":false}\n",
            (unsigned)result,zero(&candidate,sizeof(candidate))?"true":"false",wiped?"true":"false");return 1;
    }
    /* Successful native validation permits only identifier/SemVer/hex/channel
     * characters in the strings below; no untrusted JSON text is echoed. */
    (void)printf("{\"kind\":\"tv4-motion-host-review-1\",\"status\":\"verified-offline-only\",\"result\":0,\"release_version\":\"%s\",\"component_version\":\"%s\",\"source_commit\":\"%s\",\"channel\":\"%s\",\"key_id\":\"%s\",\"image_bytes\":%" PRIu32 ",\"image_counter\":%" PRIu32 ",\"sha256\":\"",
        candidate.release_version,candidate.component_version,candidate.source_commit,candidate.channel,candidate.image.key_id,
        candidate.image.image_bytes,candidate.image.image_counter);
    hex(candidate.image.image_sha256);fputs("\",\"metadata_sha256\":\"",stdout);hex(candidate.image.metadata_sha256);
    fputs("\",\"workspace_zero\":true,\"device_access\":false,\"install_enabled\":false}\n",stdout);return 0;
}
