#include "staging_source.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

enum { CAP=1025 };
typedef struct {
    tv4_boot_stage stage;
    tv4_boot_stage_config config;
    tv4_update_rx rx;
    uint8_t region[CAP+2];
    uint32_t now;
} fixture;
static void init(fixture *f)
{
    *f=(fixture){0};memset(f->region,0xcc,sizeof(f->region));f->now=10;
    f->config=(tv4_boot_stage_config){.buffer=f->region+1,.capacity=CAP,.boot=7,.object=17};
    assert(tv4_boot_stage_init(&f->stage,&f->config));
}
static void receiver(fixture *f,uint32_t id,uint32_t length,uint16_t block)
{
    tv4_update_limits l={.max_image_bytes=CAP+1,.block_bytes=block,.retry_limit=4,
        .ack_timeout_ms=2,.progress_timeout_ms=20,.total_timeout_ms=100};
    tv4_update_descriptor d={.id={7,id},.image_bytes=length,.image_sha256={1,2,3}};
    assert(tv4_update_rx_init(&f->rx,l,7,f->now));
    assert(tv4_update_rx_begin(&f->rx,&d,f->now)==TV4_UPDATE_OK);
}
static tv4_update_block pending(fixture *f)
{
    const tv4_update_context *c=&f->rx.context;
    uint32_t n=c->descriptor.image_bytes-c->next_offset;
    if(n>c->limits.block_bytes)n=c->limits.block_bytes;
    tv4_update_block b={.id=c->descriptor.id,.sequence=c->next_sequence,.offset=c->next_offset,.length=(uint16_t)n};
    for(uint32_t i=0;i<n;++i)b.bytes[i]=(uint8_t)((b.offset+i)*13u+9u);
    assert(tv4_update_block_checksum(&b,&b.crc32c));
    tv4_update_ack a;
    assert(tv4_update_rx_receive(&f->rx,&b,&a,f->now)==TV4_UPDATE_PENDING);
    return b;
}
static void transfer(fixture *f)
{
    tv4_update_ack a;
    assert(tv4_boot_stage_consume(&f->stage,&a,f->now)==TV4_UPDATE_WAIT);
    while(f->rx.context.state!=TV4_UPDATE_COMPLETE_UNVERIFIED) {
        tv4_update_block b=pending(f);
        assert(tv4_update_rx_receive(&f->rx,&b,&a,f->now)==TV4_UPDATE_PENDING);
        assert(tv4_boot_stage_consume(&f->stage,&a,f->now)==TV4_UPDATE_ACK);
        assert(a.next_offset==f->stage.written && a.next_sequence==f->stage.next_sequence);
        tv4_boot_stage snapshot=f->stage;
        assert(tv4_update_rx_receive(&f->rx,&b,&a,f->now)==TV4_UPDATE_ACK);
        assert(memcmp(&snapshot,&f->stage,sizeof(snapshot))==0);
    }
    assert(f->stage.state==TV4_STAGE_SEALED && f->stage.rx==NULL);
    assert(f->region[0]==0xcc && f->region[CAP+1]==0xcc);
    for(uint32_t i=0;i<f->stage.written;++i)assert(f->region[i+1]==(uint8_t)(i*13u+9u));
}
static void sizes_and_reads(void)
{
    const uint32_t sizes[]={1,255,256,257,CAP};const uint16_t blocks[]={1,31,256};
    const size_t fragments[]={1,7,256};unsigned variants=0;
    for(size_t i=0;i<5;++i)for(size_t j=0;j<3;++j) {
        fixture f;init(&f);receiver(&f,1,sizes[i],blocks[j]);assert(tv4_boot_stage_begin(&f.stage,&f.rx));transfer(&f);
        tv4_boot_source_view v;assert(tv4_boot_stage_acquire(&f.stage,&f.rx.context.descriptor,&v));
        assert(v.object==17 && v.generation==1 && v.id.transfer==1 && v.image_bytes==sizes[i] && v.token!=0);
        for(size_t k=0;k<3;++k) {
            uint32_t offset=0;uint8_t bytes[256];bool eof=false;size_t n=0;
            do {
                assert(tv4_boot_stage_read(&f.stage,v.token,offset,bytes,fragments[k],&n,&eof));
                for(size_t a=0;a<n;++a)assert(bytes[a]==(uint8_t)((offset+a)*13u+9u));
                offset+=(uint32_t)n;
            }while(!eof);
            assert(offset==sizes[i]);n=777;eof=false;
            assert(tv4_boot_stage_read(&f.stage,v.token,offset,bytes,1,&n,&eof) && n==0 && eof);++variants;
        }
        assert(tv4_boot_stage_release(&f.stage,v.token));assert(tv4_boot_stage_discard(&f.stage));
        for(uint32_t k=0;k<sizes[i];++k)assert(f.region[k+1]==0);
        for(size_t k=sizes[i]+1;k<CAP+2u;++k)assert(f.region[k]==0xcc);
    }
    printf("PASS %u RAM staging/read variants, 1..1025 bytes, blocks1/31/256, fragments1/7/256, pending/final duplicates\n",variants);
}
static void lease_and_reuse(void)
{
    fixture f;init(&f);receiver(&f,1,257,31);assert(tv4_boot_stage_begin(&f.stage,&f.rx));transfer(&f);
    tv4_boot_source_view first,second;assert(tv4_boot_stage_acquire(&f.stage,&f.rx.context.descriptor,&first));
    tv4_boot_stage snapshot=f.stage;tv4_update_ack ack;uint8_t region[sizeof(f.region)];memcpy(region,f.region,sizeof(region));
    assert(!tv4_boot_stage_discard(&f.stage) && !tv4_boot_stage_init(&f.stage,&f.config));
    assert(!tv4_boot_stage_begin(&f.stage,&f.rx));
    assert(tv4_boot_stage_consume(&f.stage,&ack,f.now)==TV4_UPDATE_REJECT_STATE);
    assert(!tv4_boot_stage_acquire(&f.stage,&f.rx.context.descriptor,&second));
    assert(memcmp(&snapshot,&f.stage,sizeof(snapshot))==0 && memcmp(region,f.region,sizeof(region))==0);
    assert(!tv4_boot_stage_release(&f.stage,first.token+1));
    assert(tv4_boot_stage_release(&f.stage,first.token));assert(tv4_boot_stage_release(&f.stage,first.token));
    assert(tv4_boot_stage_acquire(&f.stage,&f.rx.context.descriptor,&second) && second.token>first.token);
    assert(!tv4_boot_stage_release(&f.stage,first.token));
    assert(!tv4_boot_stage_inspect(&f.stage,first.token,&first));
    assert(tv4_boot_stage_inspect(&f.stage,second.token,&first));
    assert(tv4_boot_stage_release(&f.stage,second.token));assert(tv4_boot_stage_discard(&f.stage));
    assert(f.stage.last_transfer==1 && f.stage.generation==1 && f.stage.token_counter==2);
    assert(!tv4_boot_stage_release(&f.stage,second.token));
    receiver(&f,1,1,1);assert(!tv4_boot_stage_begin(&f.stage,&f.rx));
    receiver(&f,2,1,1);assert(tv4_boot_stage_begin(&f.stage,&f.rx));transfer(&f);
    assert(f.stage.generation==2 && f.stage.last_transfer==2);
}
static void receiver_faults(void)
{
    for(unsigned i=0;i<10;++i) {
        fixture f;init(&f);receiver(&f,1,77,31);assert(tv4_boot_stage_begin(&f.stage,&f.rx));
        (void)pending(&f);tv4_update_ack ack;
        assert(tv4_boot_stage_consume(&f.stage,&ack,f.now)==TV4_UPDATE_ACK);(void)pending(&f);
        switch(i) {
        case 0: f.rx.context.descriptor.image_sha256[0]^=1;break;
        case 1: ++f.rx.context.limits.total_timeout_ms;break;
        case 2: ++f.rx.context.started_ms;break;
        case 3: ++f.rx.context.progress_ms;break;
        case 4: ++f.rx.context.next_offset;break;
        case 5: ++f.rx.context.next_sequence;break;
        case 6: ++f.rx.context.local_boot;break;
        case 7: assert(tv4_update_rx_abort(&f.rx,f.rx.context.descriptor.id,f.now)==TV4_UPDATE_OK);break;
        case 8: f.rx.block.length=257;break;
        case 9: receiver(&f,2,77,31);break;
        default:assert(false);
        }
        memset(&ack,0xa5,sizeof(ack));tv4_update_ack before=ack;
        tv4_update_result r=tv4_boot_stage_consume(&f.stage,&ack,f.now);
        assert(r!=TV4_UPDATE_ACK && r!=TV4_UPDATE_WAIT && f.stage.state==TV4_STAGE_INVALID && f.stage.written==31);
        assert(memcmp(&before,&ack,sizeof(ack))==0);
        tv4_boot_source_view v;assert(!tv4_boot_stage_acquire(&f.stage,&f.rx.context.descriptor,&v));
        assert(tv4_boot_stage_discard(&f.stage));
        for(unsigned k=1;k<=31;++k)assert(f.region[k]==0);
        for(unsigned k=32;k<sizeof(f.region);++k)assert(f.region[k]==0xcc);
    }
}
static void deadlines_and_wrap(void)
{
    for(unsigned i=0;i<3;++i) {
        fixture f;init(&f);receiver(&f,1,77,31);assert(tv4_boot_stage_begin(&f.stage,&f.rx));(void)pending(&f);
        if(i==0)f.now+=20;else if(i==1)f.now=9;else f.now+=100;
        tv4_update_ack a;assert(tv4_boot_stage_consume(&f.stage,&a,f.now)==TV4_UPDATE_ERROR);
        assert(f.stage.state==TV4_STAGE_INVALID && f.stage.written==0 && f.rx.context.progress_ms==10);
        assert(f.rx.context.error==(i==1?TV4_UPDATE_ERROR_CLOCK:i==2?TV4_UPDATE_ERROR_TOTAL_TIMEOUT:TV4_UPDATE_ERROR_PROGRESS_TIMEOUT));
        assert(tv4_boot_stage_discard(&f.stage) && f.stage.last_transfer==1);
    }
    fixture f;init(&f);f.now=UINT32_MAX-5u;receiver(&f,1,77,31);assert(tv4_boot_stage_begin(&f.stage,&f.rx));
    f.now+=10;transfer(&f);assert(f.rx.context.started_ms==UINT32_MAX-5u && f.rx.context.progress_ms==4);
}
static void arguments_and_aliases(void)
{
    fixture f;init(&f);receiver(&f,1,77,31);assert(tv4_boot_stage_begin(&f.stage,&f.rx));(void)pending(&f);
    tv4_boot_stage snapshot=f.stage;tv4_update_rx rx=f.rx;
    assert(tv4_boot_stage_consume(&f.stage,(tv4_update_ack *)(void *)&f.stage,f.now)==TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_boot_stage_consume(&f.stage,(tv4_update_ack *)(void *)&f.rx,f.now)==TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_boot_stage_consume(&f.stage,(tv4_update_ack *)(void *)f.config.buffer,f.now)==TV4_UPDATE_REJECT_ARGUMENT);
    assert(memcmp(&snapshot,&f.stage,sizeof(snapshot))==0 && memcmp(&rx,&f.rx,sizeof(rx))==0);
    tv4_update_ack a;assert(tv4_boot_stage_consume(&f.stage,&a,f.now)==TV4_UPDATE_ACK);
    while(f.stage.state==TV4_STAGE_RECEIVING){(void)pending(&f);assert(tv4_boot_stage_consume(&f.stage,&a,f.now)==TV4_UPDATE_ACK);}
    tv4_boot_source_view v;tv4_update_descriptor d=f.rx.context.descriptor;
    ++d.image_bytes;assert(!tv4_boot_stage_acquire(&f.stage,&d,&v));d=f.rx.context.descriptor;d.image_sha256[0]^=1;
    assert(!tv4_boot_stage_acquire(&f.stage,&d,&v));
    assert(!tv4_boot_stage_acquire(&f.stage,&f.stage.descriptor,&v));
    assert(!tv4_boot_stage_acquire(&f.stage,&f.rx.context.descriptor,(tv4_boot_source_view *)(void *)f.config.buffer));
    assert(tv4_boot_stage_acquire(&f.stage,&f.rx.context.descriptor,&v));snapshot=f.stage;
    uint8_t bytes[257];memset(bytes,0xa5,sizeof(bytes));size_t count=777;bool eof=false;
    assert(!tv4_boot_stage_read(&f.stage,v.token,0,bytes,257,&count,&eof) && count==777 && !eof && bytes[0]==0xa5);
    assert(!tv4_boot_stage_read(&f.stage,v.token,UINT32_MAX,bytes,1,&count,&eof));
    assert(!tv4_boot_stage_read(&f.stage,v.token,0,f.config.buffer,1,&count,&eof));
    assert(!tv4_boot_stage_read(&f.stage,v.token,0,bytes,1,(size_t *)(void *)f.config.buffer,&eof));
    assert(!tv4_boot_stage_read(&f.stage,v.token,0,bytes,8,(size_t *)(void *)bytes,&eof));
    assert(!tv4_boot_stage_read(&f.stage,v.token,0,bytes,1,&count,(bool *)(void *)&count));
    assert(!tv4_boot_stage_inspect(&f.stage,v.token,(tv4_boot_source_view *)(void *)&f.stage));
    assert(!tv4_boot_stage_inspect(&f.stage,v.token,(tv4_boot_source_view *)(uintptr_t)(UINTPTR_MAX-4u)));
    assert(memcmp(&snapshot,&f.stage,sizeof(snapshot))==0 && count==777 && !eof);
    assert(tv4_boot_stage_release(&f.stage,v.token));
}
static void config_and_capacity(void)
{
    for(unsigned i=0;i<6;++i) {
        fixture f={0};tv4_boot_stage_config c={.buffer=f.region,.capacity=CAP,.boot=7,.object=17};
        if(i==0)c.buffer=NULL;
        if(i==1)c.capacity=0;
        if(i==2)c.boot=0;
        if(i==3)c.object=0;
        if(i==4)c.buffer=(uint8_t *)(void *)&f.stage;
        if(i==5)c.buffer=(uint8_t *)(void *)&c;
        assert(!tv4_boot_stage_init(&f.stage,&c) && !f.stage.configured);
    }
    fixture f;init(&f);receiver(&f,1,CAP+1,256);tv4_boot_stage snapshot=f.stage;
    assert(!tv4_boot_stage_begin(&f.stage,&f.rx) && memcmp(&snapshot,&f.stage,sizeof(snapshot))==0);
    assert(!tv4_boot_stage_begin(&f.stage,(tv4_update_rx *)(void *)f.config.buffer));
    receiver(&f,1,1,1);f.rx.context.descriptor.id.boot=8;assert(!tv4_boot_stage_begin(&f.stage,&f.rx));
    assert(!tv4_boot_stage_init(NULL,&f.config) && !tv4_boot_stage_begin(NULL,&f.rx) && !tv4_boot_stage_discard(NULL));
}
static void counters_and_exhaustion(void)
{
    fixture f;init(&f);
    for(uint32_t i=1;i<=1536;++i) {
        receiver(&f,i,1,1);assert(tv4_boot_stage_begin(&f.stage,&f.rx));transfer(&f);
        tv4_boot_source_view v;assert(tv4_boot_stage_acquire(&f.stage,&f.rx.context.descriptor,&v));
        assert(v.generation==i && v.token==i);assert(tv4_boot_stage_release(&f.stage,v.token));assert(tv4_boot_stage_discard(&f.stage));
        f.region[1]=0xcc;
    }
    assert(f.stage.last_transfer==1536 && f.stage.generation==1536 && f.stage.token_counter==1536);
    /* Test-only mutation to reach integer boundaries, never a runtime reset API. */
    f.stage.generation=UINT64_MAX;receiver(&f,1537,1,1);assert(!tv4_boot_stage_begin(&f.stage,&f.rx));
    init(&f);f.stage.last_transfer=UINT32_MAX-1u;receiver(&f,UINT32_MAX,1,1);
    assert(tv4_boot_stage_begin(&f.stage,&f.rx));transfer(&f);f.stage.token_counter=UINT64_MAX;
    tv4_boot_source_view v;assert(!tv4_boot_stage_acquire(&f.stage,&f.rx.context.descriptor,&v));
    assert(f.stage.state==TV4_STAGE_SEALED && tv4_boot_stage_discard(&f.stage));
    receiver(&f,1,1,1);assert(!tv4_boot_stage_begin(&f.stage,&f.rx));
    puts("PASS 1536 source cycles; discard retains high-water/generation/token counters; exhaustion never wraps");
}
#define RUN(name) do{name();puts("PASS " #name);}while(0)
int main(void)
{
    RUN(sizes_and_reads);RUN(lease_and_reuse);RUN(receiver_faults);RUN(deadlines_and_wrap);
    RUN(arguments_and_aliases);RUN(config_and_capacity);RUN(counters_and_exhaustion);
    printf("PASS 7 staging source groups; state=%zu bytes plus explicit caller buffer; RAM only, not persistent/install/device access\n",sizeof(tv4_boot_stage));
    return 0;
}
