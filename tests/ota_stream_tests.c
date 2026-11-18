#include "image_stream.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static uint8_t image[4096];
static size_t size,checksum_offset;
static tv4_ota_candidate candidate;
static const tv4_image_limits limits={100,1000,2,true};
static void put32(uint8_t *p,uint32_t n) {for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static void fixture(void) {
    memset(image,0,sizeof(image));image[0]=0xe9;image[1]=2;image[2]=2;image[3]=0x4f;
    image[12]=9;image[17]=99;image[23]=1;
    put32(image+24,0x3c000020);put32(image+28,320);
    put32(image+32,0xabcd5432);strcpy((char *)image+48,"0.2.0");strcpy((char *)image+80,"tv4_hmi");
    for(size_t i=288;i<352;i++)image[i]=(uint8_t)i;
    put32(image+352,0x3fc90000);put32(image+356,80);
    for(size_t i=360;i<440;i++)image[i]=(uint8_t)(i*3);
    uint8_t check=0xef;
    for(size_t i=32;i<352;i++)check^=image[i];
    for(size_t i=360;i<440;i++)check^=image[i];
    checksum_offset=447;image[checksum_offset]=check;
    assert(tv4_ota_sha256(image,checksum_offset+1,image+checksum_offset+1));size=480;
    memset(&candidate,0,sizeof(candidate));candidate.size_bytes=(uint32_t)size;strcpy(candidate.version,"0.2.0");
    assert(tv4_ota_sha256(image,size,candidate.sha256));
}
static void no_crypto(const tv4_image_stream *s) {assert(!s->whole_hash.context && !s->body_hash.context);}
static void run_chunks(size_t chunk,uint32_t start) {
    tv4_image_stream stream={0};assert(tv4_image_start(&stream,&candidate,limits,start));
    assert(!tv4_image_start(&stream,&candidate,limits,start)); /* no silent restart */
    for(size_t i=0;i<size;i+=chunk) {
        size_t n=size-i;if(n>chunk)n=chunk;
        assert(tv4_image_feed(&stream,image+i,n,start+1));
        assert(stream.state==TV4_IMAGE_RECEIVING);
    }
    assert(tv4_image_finish(&stream,start+2));assert(stream.state==TV4_IMAGE_VERIFIED);no_crypto(&stream);
    assert(!tv4_image_finish(&stream,start+2));assert(stream.state==TV4_IMAGE_VERIFIED);
    tv4_image_close(&stream);assert(stream.state==TV4_IMAGE_IDLE);
}
static void damaged(tv4_image_error expected) {
    assert(tv4_ota_sha256(image,size,candidate.sha256)); /* simulate authenticated but bad-format image */
    tv4_image_stream stream={0};assert(tv4_image_start(&stream,&candidate,limits,0));
    bool fed=tv4_image_feed(&stream,image,size,1);
    if(fed)assert(!tv4_image_finish(&stream,2));
    assert(stream.state==TV4_IMAGE_FAILED && stream.error==expected);no_crypto(&stream);tv4_image_close(&stream);
}
static void checks(void) {
    fixture();image[0]=0;damaged(TV4_IMAGE_HEADER);
    fixture();image[1]=0;damaged(TV4_IMAGE_HEADER);
    fixture();image[1]=17;damaged(TV4_IMAGE_HEADER);
    fixture();image[3]=0x3f;damaged(TV4_IMAGE_HEADER);
    fixture();image[12]=0;damaged(TV4_IMAGE_HEADER);
    fixture();image[15]=3;damaged(TV4_IMAGE_HEADER);
    fixture();image[17]=1;damaged(TV4_IMAGE_HEADER);
    fixture();image[23]=0;damaged(TV4_IMAGE_HEADER);
    fixture();put32(image+28,0xfffffffc);damaged(TV4_IMAGE_SEGMENT);
    fixture();put32(image+28,255);damaged(TV4_IMAGE_SEGMENT);
    fixture();put32(image+32,0);damaged(TV4_IMAGE_DESCRIPTOR);
    fixture();put32(image+36,1);damaged(TV4_IMAGE_DESCRIPTOR);
    fixture();image[48]='1';damaged(TV4_IMAGE_DESCRIPTOR);
    fixture();memset(image+48,'x',32);damaged(TV4_IMAGE_DESCRIPTOR);
    fixture();image[80]='X';damaged(TV4_IMAGE_DESCRIPTOR);
    fixture();image[440]=1;damaged(TV4_IMAGE_CHECKSUM);
    fixture();image[447]^=1;damaged(TV4_IMAGE_CHECKSUM);
    fixture();image[479]^=1;damaged(TV4_IMAGE_DIGEST);
    fixture();candidate.sha256[0]^=1;
    tv4_image_stream stream={0};assert(tv4_image_start(&stream,&candidate,limits,0));
    assert(tv4_image_feed(&stream,image,size,1));assert(!tv4_image_finish(&stream,2));
    assert(stream.error==TV4_IMAGE_DIGEST);no_crypto(&stream);tv4_image_close(&stream);
}
static void interruptions(void) {
    fixture();tv4_image_stream stream={0};
    for(size_t cut=0;cut<size;cut++) {
        assert(tv4_image_start(&stream,&candidate,limits,0));
        if(cut)assert(tv4_image_feed(&stream,image,cut,1));
        assert(!tv4_image_finish(&stream,2));assert(stream.error==TV4_IMAGE_LENGTH);no_crypto(&stream);
        tv4_image_close(&stream);
    }
    for(size_t cut=0;cut<size;cut+=17) {
        assert(tv4_image_start(&stream,&candidate,limits,0));
        if(cut)assert(tv4_image_feed(&stream,image,cut,1));
        tv4_image_abort(&stream,true);assert(stream.state==TV4_IMAGE_CANCELLED);no_crypto(&stream);
        assert(!tv4_image_feed(&stream,image,size,2));tv4_image_close(&stream);
    }
    assert(tv4_image_start(&stream,&candidate,limits,0));tv4_image_abort(&stream,false);
    assert(stream.state==TV4_IMAGE_FAILED && stream.error==TV4_IMAGE_IO);no_crypto(&stream);tv4_image_close(&stream);
    assert(tv4_image_start(&stream,&candidate,limits,0));
    assert(!tv4_image_feed(&stream,image,1,100)); /* late data cannot hide expired idle deadline */
    assert(stream.error==TV4_IMAGE_TIMEOUT);no_crypto(&stream);tv4_image_close(&stream);
    assert(tv4_image_start(&stream,&candidate,limits,UINT32_MAX-50));
    assert(tv4_image_feed(&stream,image,1,10));assert(!tv4_image_poll(&stream,110));
    assert(stream.error==TV4_IMAGE_TIMEOUT);no_crypto(&stream);tv4_image_close(&stream);
    assert(tv4_image_start(&stream,&candidate,limits,0));
    for(uint32_t time=90;time<1000;time+=90)assert(tv4_image_feed(&stream,image+(time/90-1),1,time));
    assert(!tv4_image_feed(&stream,image+11,1,1000));assert(stream.error==TV4_IMAGE_TIMEOUT);
    no_crypto(&stream);tv4_image_close(&stream);
}
static void arguments(void) {
    fixture();tv4_image_stream stream={0};tv4_image_limits bad=limits;bad.chip_revision_known=false;
    assert(!tv4_image_start(&stream,&candidate,bad,0));no_crypto(&stream);tv4_image_close(&stream);
    bad=limits;bad.idle_timeout_ms=0;assert(!tv4_image_start(&stream,&candidate,bad,0));tv4_image_close(&stream);
    bad=limits;bad.total_timeout_ms=UINT32_MAX;assert(!tv4_image_start(&stream,&candidate,bad,0));tv4_image_close(&stream);
    strcpy(candidate.version,"012345678901234567890123456789012345");
    assert(!tv4_image_start(&stream,&candidate,limits,0));tv4_image_close(&stream);fixture();
    assert(tv4_image_start(&stream,&candidate,limits,0));assert(!tv4_image_feed(&stream,NULL,1,1));
    assert(stream.error==TV4_IMAGE_ARGUMENT);no_crypto(&stream);tv4_image_close(&stream);
    assert(tv4_image_start(&stream,&candidate,limits,0));assert(!tv4_image_feed(&stream,image,0,1));
    no_crypto(&stream);tv4_image_close(&stream);
    assert(tv4_image_start(&stream,&candidate,limits,0));assert(!tv4_image_feed(&stream,image,4097,1));
    no_crypto(&stream);tv4_image_close(&stream);
    assert(tv4_image_start(&stream,&candidate,limits,0));assert(!tv4_image_feed(&stream,image,size+1,1));
    assert(stream.error==TV4_IMAGE_LENGTH);no_crypto(&stream);tv4_image_close(&stream);
}
int main(int argc,char **argv) {
    fixture();const size_t chunks[]={1,7,23,24,31,255,256,4096};
    for(unsigned i=0;i<sizeof(chunks)/sizeof(chunks[0]);i++)run_chunks(chunks[i],0);
    run_chunks(7,UINT32_MAX-10);checks();interruptions();arguments();
    if(argc==3) {
        /* Actual SDK app artifact: format/digest smoke test, NOT approval.
         * SDK builds currently have git-derived versions, outside manifest SemVer. */
        FILE *f=fopen(argv[1],"rb");assert(f);char *end;unsigned long bytes=strtoul(argv[2],&end,10);assert(!*end && bytes>=321 && bytes<=0x300000);
        uint8_t *raw=malloc(bytes);assert(raw);assert(fread(raw,1,bytes,f)==bytes);assert(fgetc(f)==EOF);fclose(f);
        memset(&candidate,0,sizeof(candidate));candidate.size_bytes=(uint32_t)bytes;
        assert(memchr(raw+48,0,32));strcpy(candidate.version,(const char *)raw+48);
        assert(tv4_ota_sha256(raw,bytes,candidate.sha256));tv4_image_stream stream={0};assert(tv4_image_start(&stream,&candidate,limits,0));
        for(size_t at=0;at<bytes;at+=4096) {size_t n=bytes-at;if(n>4096)n=4096;assert(tv4_image_feed(&stream,raw+at,n,1));}
        assert(tv4_image_finish(&stream,2));tv4_image_close(&stream);free(raw);
        puts("Actual SDK app streamed format/digest check PASS (not install approval)");
    }
    printf("Stream chunks, descriptor/checksum/dual-SHA, 480 truncation points, cancellation/deadlines/wrap PASS; verifier struct=%zu bytes\n",sizeof(tv4_image_stream));
}
