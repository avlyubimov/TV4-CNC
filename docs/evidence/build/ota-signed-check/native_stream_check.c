/* Evidence caller of EXISTING host primitives; no firmware, network or sink. */
#include "envelope.h"
#include "image_stream.h"
#include <stdio.h>
#include <string.h>
static unsigned char raw[TV4_OTA_MANIFEST_MAX+1],sig[TV4_OTA_ENVELOPE_MAX+1],pub[92],chunk[4096];
static size_t load(const char *name,unsigned char *out,size_t limit) {
    FILE *f=fopen(name,"rb");if(!f)return 0;
    size_t n=fread(out,1,limit,f);int bad=ferror(f);fclose(f);return bad?0:n;
}
int main(int argc,char **argv) {
    if(argc!=9)return 2;
    size_t n=load(argv[1],raw,sizeof(raw)),s=load(argv[2],sig,sizeof(sig)),p=load(argv[3],pub,sizeof(pub));
    tv4_ota_context ctx={argv[5],argv[6],argv[7],argv[8],NULL,"tv4-16m-ab-v1",true,true,pub,p};
    tv4_ota_candidate candidate={0};
    tv4_ota_result policy=tv4_ota_signed_manifest_read(raw,n,sig,s,&ctx,&candidate);
    if(policy!=TV4_OTA_OK){printf("{\"manifest_result\":%u,\"device_access\":false}\n",(unsigned)policy);return 1;}
    tv4_image_stream stream={0};
    /* Known target rev0.2 from the previous preflight; synthetic monotonic ticks.
       This is format/crypto evidence, not hardware timing or resource admission. */
    tv4_image_limits limits={15000,300000,2,true};
    if(!tv4_image_start(&stream,&candidate,limits,0))return 3;
    FILE *f=fopen(argv[4],"rb");if(!f){tv4_image_close(&stream);return 4;}
    size_t size;unsigned total=0,tick=1;int ok=1;
    while((size=fread(chunk,1,sizeof(chunk),f))>0) {
        if(!tv4_image_feed(&stream,chunk,size,tick++)){ok=0;break;}
        total+=(unsigned)size;
    }
    if(ferror(f))ok=0;
    fclose(f);
    if(ok)ok=tv4_image_finish(&stream,tick);
    unsigned state=(unsigned)stream.state,error=(unsigned)stream.error;
    tv4_image_close(&stream);
    int closed=!stream.whole_hash.context && !stream.body_hash.context;
    printf("{\"manifest_result\":0,\"image_verified\":%s,\"received\":%u,\"state\":%u,\"error\":%u,\"contexts_closed\":%s,\"chunk_bytes\":4096,\"synthetic_clock\":true,\"device_access\":false}\n",
        ok?"true":"false",total,state,error,closed?"true":"false");
    return ok && closed?0:1;
}
