#include "update_fragments.h"
#include "update_metadata_setup.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static const tv4_fragment_config cfg={10,1000,256,TV4_FRAGMENT_VERSION};
static bool zero(const void *p,size_t n)
{ const uint8_t *b=p;for(size_t i=0;i<n;++i)if(b[i]!=0)return false;return true; }
static void deliver(tv4_fragment_tx *tx,tv4_fragment_rx *rx,uint32_t now)
{
    while(tx->storage.state==TV4_FRAG_SENDING) {
        tv4_fragment_frame frame;assert(tv4_fragment_tx_peek(tx,&frame,11,now)==TV4_FRAG_FRAME);
        tv4_fragment_result r=tv4_fragment_rx_receive(rx,7,&frame,11,now);
        assert(r==TV4_FRAG_PENDING || r==TV4_FRAG_READY);
        r=tv4_fragment_tx_commit(tx,&frame,11,now);assert(r==TV4_FRAG_COMMITTED || r==TV4_FRAG_FINISHED);
    }
}
static void lengths_and_modes(void)
{
    tv4_fragment_tx tx={0};tv4_fragment_rx rx={0};uint8_t raw[286],out[286];size_t n;
    for(unsigned i=0;i<286;++i)raw[i]=(uint8_t)i;
    assert(tv4_fragment_tx_init_raw(&tx,&cfg,7,11,0,0)==TV4_FRAG_OK);
    assert(tv4_fragment_rx_init_raw(&rx,&cfg,7,11,0,0)==TV4_FRAG_OK);
    tv4_wire_message typed={.kind=TV4_WIRE_DATA_ACK,.body.ack={.id={1,2}}};
    assert(tv4_fragment_tx_begin(&tx,&typed,11,0)==TV4_FRAG_STATE);
    assert(tv4_fragment_tx_init(&tx,&cfg,7,11,0,0)==TV4_FRAG_STATE && tx.storage.raw_mode);
    for(size_t size=8;size<=286;++size) {
        assert(tv4_fragment_tx_begin_raw(&tx,raw,size,11,0)==TV4_FRAG_OK);deliver(&tx,&rx,0);
        tv4_wire_message rejected;memset(&rejected,0xa5,sizeof(rejected));
        assert(tv4_fragment_rx_take(&rx,&rejected,11,0)==TV4_FRAG_STATE && zero(&rejected,sizeof(rejected)));
        for(size_t cap=0;cap<size;++cap) {
            memset(out,0xa5,sizeof(out));n=99;
            assert(tv4_fragment_rx_take_raw(&rx,out,cap,&n,11,0)==TV4_FRAG_LIMIT && n==0);
            for(size_t i=0;i<sizeof(out);++i)assert(out[i]==0xa5);
            assert(rx.storage.state==TV4_FRAG_READY_BYTES_UNCHECKED);
        }
        assert(tv4_fragment_rx_take_raw(&rx,out,sizeof(out),&n,11,0)==TV4_FRAG_RAW_TAKEN_UNCHECKED);
        assert(n==size && !memcmp(out,raw,n) && zero(rx.storage.bytes,sizeof(rx.storage.bytes)));
        n=99;assert(tv4_fragment_rx_take_raw(&rx,out,sizeof(out),&n,11,0)==TV4_FRAG_STATE && n==0);
        assert(tx.storage.counter==size-7u && rx.storage.counter==tx.storage.counter);
    }
    assert(tv4_fragment_tx_cancel(&tx,11,0)==TV4_FRAG_OK && tv4_fragment_tx_clear(&tx,11,0)==TV4_FRAG_OK);
    assert(tv4_fragment_rx_cancel(&rx,11,0)==TV4_FRAG_OK && tv4_fragment_rx_clear(&rx,11,0)==TV4_FRAG_OK);
    assert(tx.storage.raw_mode && rx.storage.raw_mode && tx.storage.counter==279);
    for(size_t size=0;size<8;++size)assert(tv4_fragment_tx_begin_raw(&tx,raw,size,11,0)==TV4_FRAG_LIMIT);
    assert(tv4_fragment_tx_begin_raw(&tx,raw,287,11,0)==TV4_FRAG_LIMIT && tx.storage.counter==279);
    tv4_fragment_tx strict={0};tv4_fragment_rx strict_rx={0};
    assert(tv4_fragment_tx_init(&strict,&cfg,7,11,0,0)==TV4_FRAG_OK);
    assert(tv4_fragment_rx_init(&strict_rx,&cfg,7,11,0,0)==TV4_FRAG_OK);
    assert(tv4_fragment_tx_begin_raw(&strict,raw,8,11,0)==TV4_FRAG_STATE);
    assert(tv4_fragment_rx_take_raw(&strict_rx,out,sizeof(out),&n,11,0)==TV4_FRAG_STATE && n==0);
    puts("PASS279 raw lengths8..286, all undersized capacities, mode isolation, single take, retained IDs");
}
static void codecs_and_faults(void)
{
    tv4_metadata_setup_message setup={.kind=TV4_NEG_REQUEST,.request_id={11,1},
        .binding={.image={.id={22,1},.image_bytes=1000}},.metadata={.image_bytes=98},.block_bytes=64};
    uint8_t raw[286],out[286];size_t n;
    assert(tv4_metadata_setup_encode(&setup,raw,sizeof(raw),&n)==TV4_SETUP_OK);
    tv4_fragment_tx tx={0};tv4_fragment_rx rx={0};
    assert(tv4_fragment_tx_init_raw(&tx,&cfg,7,11,0,0)==TV4_FRAG_OK);
    assert(tv4_fragment_rx_init(&rx,&cfg,7,11,0,0)==TV4_FRAG_OK);
    assert(tv4_fragment_tx_begin_raw(&tx,raw,n,11,0)==TV4_FRAG_OK);deliver(&tx,&rx,0);
    tv4_wire_message typed;
    assert(tv4_fragment_rx_take(&rx,&typed,11,0)==TV4_FRAG_ERROR && rx.storage.error==TV4_FRAG_CODEC && zero(&typed,sizeof(typed)));
    tv4_wire_message ack={.kind=TV4_WIRE_DATA_ACK,.body.ack={.id={1,2}}};
    assert(tv4_update_wire_encode(&ack,raw,sizeof(raw),&n)==TV4_WIRE_OK);
    assert(tv4_metadata_setup_decode(raw,n,&setup)!=TV4_SETUP_OK && zero(&setup,sizeof(setup)));
    for(unsigned which=0;which<4;++which) {
        tv4_fragment_tx a={0};tv4_fragment_rx b={0};uint32_t now=100;
        assert(tv4_fragment_tx_init_raw(&a,&cfg,7,11,which==3?UINT16_MAX:0,now)==TV4_FRAG_OK);
        assert(tv4_fragment_rx_init_raw(&b,&cfg,7,11,0,now)==TV4_FRAG_OK);
        if(which==3) {
            assert(tv4_fragment_tx_begin_raw(&a,raw,8,11,now)==TV4_FRAG_ERROR && a.storage.error==TV4_FRAG_EXHAUSTED);
            assert(tv4_fragment_tx_clear(&a,11,now)==TV4_FRAG_ERROR);continue;
        }
        assert(tv4_fragment_tx_begin_raw(&a,raw,8,11,now)==TV4_FRAG_OK);
        if(which==0) {
            now+=10;assert(tv4_fragment_tx_poll(&a,11,now)==TV4_FRAG_ERROR && a.storage.error==TV4_FRAG_PROGRESS_TIMEOUT);
            assert(tv4_fragment_tx_clear(&a,11,now)==TV4_FRAG_OK && a.storage.counter==1 && a.storage.raw_mode);
        }else if(which==1) {
            assert(tv4_fragment_tx_poll(&a,12,now)==TV4_FRAG_ERROR);
            assert(tv4_fragment_tx_clear(&a,11,now)==TV4_FRAG_ERROR);
        }else {
            deliver(&a,&b,now);--now;n=99;memset(out,0xa5,sizeof(out));
            assert(tv4_fragment_rx_take_raw(&b,out,sizeof(out),&n,11,now)==TV4_FRAG_ERROR && n==0);
            assert(out[0]==0xa5 && tv4_fragment_rx_clear(&b,11,now)==TV4_FRAG_ERROR);
        }
    }
    puts("PASS raw/typed codec mismatch, context/clock/timeout/exhaustion; no authority from raw bytes");
}
int main(void){lengths_and_modes();codecs_and_faults();return 0;}
