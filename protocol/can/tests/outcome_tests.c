#include "update_outcome.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    tv4_update_rx data_rx;
    tv4_update_tx data_tx;
    tv4_outcome_tx tx;
    tv4_outcome_rx rx;
    tv4_update_outcome report,packet,ack,delivery;
    tv4_outcome_limits limits;
    uint32_t now;
} fixture;
static bool zero(const void *p,size_t n)
{
    const uint8_t *bytes=p;
    for (size_t i=0;i<n;++i) if (bytes[i]!=0) return false;
    return true;
}
static void setup(fixture *f,uint32_t now,uint32_t boot)
{
    *f=(fixture){.now=now,.limits={.ack_timeout_ms=10,.linger_ms=50,.total_timeout_ms=100,.retry_limit=3}};
    f->report=(tv4_update_outcome){.binding={.image={.id={boot,42},.image_bytes=257}},
        .code=TV4_OUTCOME_CHECKED_CLOSED,.reason=TV4_OUTCOME_REASON_NONE};
    for (size_t i=0;i<32;++i) {
        f->report.binding.image.image_sha256[i]=(uint8_t)i;
        f->report.binding.metadata_sha256[i]=(uint8_t)(i+32u);
    }
    tv4_update_limits l={.max_image_bytes=1024,.ack_timeout_ms=10,.progress_timeout_ms=1000,.total_timeout_ms=10000,.block_bytes=128,.retry_limit=3};
    assert(tv4_update_tx_init(&f->data_tx,l,boot,now) && tv4_update_rx_init(&f->data_rx,l,boot,now));
    assert(tv4_update_tx_begin(&f->data_tx,&f->report.binding.image,now)==TV4_UPDATE_OK);
    assert(tv4_update_rx_begin(&f->data_rx,&f->report.binding.image,now)==TV4_UPDATE_OK);
    assert(tv4_outcome_tx_init(&f->tx,f->limits,boot,now)); assert(tv4_outcome_rx_init(&f->rx,f->limits,now));
}
static void complete_data(fixture *f)
{
    uint8_t bytes[128]={0};
    while (f->data_tx.context.state!=TV4_UPDATE_COMPLETE_UNVERIFIED) {
        size_t n=257u-f->data_tx.context.next_offset; if (n>128) n=128;
        tv4_update_block packet; tv4_update_ack ack;
        assert(tv4_update_tx_load(&f->data_tx,bytes,n,f->now)==TV4_UPDATE_OK);
        assert(tv4_update_tx_next(&f->data_tx,&packet,f->now)==TV4_UPDATE_OK);
        assert(tv4_update_rx_receive(&f->data_rx,&packet,&ack,f->now)==TV4_UPDATE_PENDING);
        assert(tv4_update_rx_consume(&f->data_rx,packet.id,packet.sequence,true,&ack,f->now)==TV4_UPDATE_ACK);
        assert(tv4_update_tx_ack(&f->data_tx,&ack,f->now)==TV4_UPDATE_OK);
    }
}
static void begin(fixture *f)
{
    complete_data(f);
    assert(tv4_outcome_tx_begin(&f->tx,&f->report,f->now)==TV4_OUTCOME_OK);
    assert(tv4_outcome_rx_begin(&f->rx,&f->data_tx,f->report.binding.metadata_sha256,f->now)==TV4_OUTCOME_OK);
}
static void loss_duplicate_and_history(void)
{
    unsigned sessions=0,deliveries=0;
    for (unsigned i=0;i<1001;++i) {
        fixture f; setup(&f,10,i+1u);
        if (i%3u==1u) { f.report.code=TV4_OUTCOME_REJECTED_CLOSED; f.report.reason=TV4_OUTCOME_SIGNATURE; }
        if (i%3u==2u) { f.report.code=TV4_OUTCOME_QUARANTINED; f.report.reason=TV4_OUTCOME_RELEASE; }
        begin(&f);
        assert(tv4_outcome_tx_next(&f.tx,&f.packet,f.now)==TV4_OUTCOME_SENT); /* lost report */
        f.now+=10;
        assert(tv4_outcome_tx_next(&f.tx,&f.packet,f.now)==TV4_OUTCOME_SENT);
        assert(tv4_outcome_rx_receive(&f.rx,&f.packet,&f.ack,&f.delivery,f.now)==TV4_OUTCOME_DELIVERED);
        ++deliveries; assert(tv4_update_outcome_equal(&f.delivery,&f.report)); /* lost ACK */
        tv4_update_outcome before=f.delivery; f.now+=10;
        assert(tv4_outcome_tx_next(&f.tx,&f.packet,f.now)==TV4_OUTCOME_SENT);
        assert(tv4_outcome_rx_receive(&f.rx,&f.packet,&f.ack,&f.delivery,f.now)==TV4_OUTCOME_ACK);
        assert(tv4_update_outcome_equal(&before,&f.delivery));
        assert(tv4_outcome_tx_ack(&f.tx,&f.ack,f.now)==TV4_OUTCOME_OK);
        assert(tv4_outcome_tx_ack(&f.tx,&f.ack,f.now)==TV4_OUTCOME_DUPLICATE);
        assert(tv4_outcome_tx_poll(&f.tx,f.now)==TV4_OUTCOME_DONE);
        assert(tv4_outcome_rx_poll(&f.rx,70)==TV4_OUTCOME_DONE); /* first report20 + linger50 */
        assert(tv4_outcome_rx_receive(&f.rx,&f.packet,&f.ack,&f.delivery,70)==TV4_OUTCOME_DONE);
        ++sessions;
    }
    assert(sessions==deliveries); printf("PASS %u outcome sessions, %u deliveries; lost report/final ACK; duplicates not redelivered\n",sessions,deliveries);
}
static void binding_and_ack_validation(void)
{
    fixture f; setup(&f,10,77); begin(&f);
    assert(tv4_outcome_tx_next(&f.tx,&f.packet,10)==TV4_OUTCOME_SENT);
    for (unsigned i=0;i<5;++i) {
        tv4_update_outcome wrong=f.packet;
        switch (i) {
        case 0: ++wrong.binding.image.id.boot; break;
        case 1: ++wrong.binding.image.id.transfer; break;
        case 2: ++wrong.binding.image.image_bytes; break;
        case 3: wrong.binding.image.image_sha256[0]^=1; break;
        default: wrong.binding.metadata_sha256[0]^=1; break;
        }
        memset(&f.delivery,0xa5,sizeof(f.delivery)); tv4_update_outcome before=f.delivery;
        assert(tv4_outcome_rx_receive(&f.rx,&wrong,&f.ack,&f.delivery,10)==TV4_OUTCOME_REJECT_BINDING);
        assert(memcmp(&before,&f.delivery,sizeof(before))==0);
        assert(tv4_outcome_tx_ack(&f.tx,&wrong,10)==TV4_OUTCOME_REJECT_BINDING);
    }
    tv4_update_outcome wrong=f.packet; wrong.code=TV4_OUTCOME_FAILED_CLOSED; wrong.reason=TV4_OUTCOME_RELEASE;
    assert(tv4_outcome_tx_ack(&f.tx,&wrong,10)==TV4_OUTCOME_REJECT_CONTENT && f.tx.context.state==TV4_OUTCOME_WAIT_ACK);
    assert(tv4_outcome_rx_receive(&f.rx,&f.packet,&f.ack,&f.delivery,10)==TV4_OUTCOME_DELIVERED);
    assert(tv4_outcome_tx_ack(&f.tx,&f.ack,10)==TV4_OUTCOME_OK);
    for (unsigned i=0;i<10;++i) assert(tv4_outcome_tx_ack(&f.tx,&wrong,10)==TV4_OUTCOME_REJECT_CONTENT);
    assert(f.tx.context.state==TV4_OUTCOME_ACKED);
    setup(&f,10,77); begin(&f); tv4_update_outcome original=f.report;
    f.report.binding.metadata_sha256[0]^=1; f.report.code=TV4_OUTCOME_FAILED_CLOSED; f.report.reason=TV4_OUTCOME_SOURCE;
    assert(tv4_outcome_tx_next(&f.tx,&f.packet,10)==TV4_OUTCOME_SENT);
    assert(tv4_update_outcome_equal(&f.packet,&original));
    assert(tv4_outcome_rx_receive(&f.rx,&f.packet,&f.ack,&f.delivery,10)==TV4_OUTCOME_DELIVERED);
}
static void conflicts_invalid_reports_and_forgery_boundary(void)
{
    fixture f; setup(&f,10,77); begin(&f);
    /* A matching peer report CAN be forged: this channel reports history only,
     * it has no crypto/Flash/motion API. Caller must never treat it as authority. */
    assert(tv4_outcome_rx_receive(&f.rx,&f.report,&f.ack,&f.delivery,10)==TV4_OUTCOME_DELIVERED);
    tv4_update_outcome conflict=f.report; conflict.code=TV4_OUTCOME_REJECTED_CLOSED; conflict.reason=TV4_OUTCOME_HASH;
    assert(tv4_outcome_rx_receive(&f.rx,&conflict,&f.ack,&f.delivery,11)==TV4_OUTCOME_ERROR);
    assert(f.rx.context.error==TV4_OUTCOME_ERROR_CONFLICT && f.rx.context.state==TV4_OUTCOME_FAILED);
    setup(&f,10,77); begin(&f); conflict=f.report; conflict.reason=TV4_OUTCOME_HASH; /* noncanonical success */
    for (unsigned i=0;i<3;++i) assert(tv4_outcome_rx_receive(&f.rx,&conflict,&f.ack,&f.delivery,10)==TV4_OUTCOME_REJECT_CONTENT);
    assert(tv4_outcome_rx_receive(&f.rx,&conflict,&f.ack,&f.delivery,10)==TV4_OUTCOME_ERROR);
    assert(f.rx.context.error==TV4_OUTCOME_ERROR_RETRIES);
}
static void deadlines_retries_and_late_begin(void)
{
    fixture f; setup(&f,10,77); begin(&f);
    assert(tv4_outcome_tx_next(&f.tx,&f.packet,10)==TV4_OUTCOME_SENT);
    for (uint32_t t=20;t<=40;t+=10) assert(tv4_outcome_tx_next(&f.tx,&f.packet,t)==TV4_OUTCOME_SENT);
    assert(tv4_outcome_tx_next(&f.tx,&f.packet,50)==TV4_OUTCOME_ERROR && f.tx.context.error==TV4_OUTCOME_ERROR_RETRIES);
    setup(&f,10,77); begin(&f);
    assert(tv4_outcome_rx_receive(&f.rx,&f.report,&f.ack,&f.delivery,10)==TV4_OUTCOME_DELIVERED);
    assert(tv4_outcome_rx_receive(&f.rx,&f.report,&f.ack,&f.delivery,30)==TV4_OUTCOME_ACK);
    assert(tv4_outcome_rx_poll(&f.rx,60)==TV4_OUTCOME_DONE); /* duplicate did not reset linger */
    setup(&f,10,77); begin(&f); assert(tv4_outcome_rx_poll(&f.rx,110)==TV4_OUTCOME_ERROR && f.rx.context.error==TV4_OUTCOME_ERROR_TIMEOUT);
    setup(&f,10,77); complete_data(&f);
    assert(tv4_outcome_rx_begin(&f.rx,&f.data_tx,f.report.binding.metadata_sha256,110)==TV4_OUTCOME_ERROR);
    assert(f.rx.context.error==TV4_OUTCOME_ERROR_TIMEOUT); /* cannot restart DATA-end deadline */
}
static void transport_reboot_abort_and_clock(void)
{
    fixture f; setup(&f,10,77);
    assert(tv4_outcome_rx_begin(&f.rx,&f.data_tx,f.report.binding.metadata_sha256,10)==TV4_OUTCOME_REJECT_STATE);
    begin(&f);
    assert(tv4_update_tx_init(&f.data_tx,f.data_tx.context.limits,78,10));
    assert(tv4_outcome_rx_poll(&f.rx,10)==TV4_OUTCOME_ERROR && f.rx.context.error==TV4_OUTCOME_ERROR_TRANSPORT);
    setup(&f,10,77); begin(&f);
    assert(tv4_outcome_rx_receive(&f.rx,&f.report,&f.ack,&f.delivery,10)==TV4_OUTCOME_DELIVERED);
    assert(tv4_update_tx_abort(&f.data_tx,f.report.binding.image.id,10)==TV4_UPDATE_OK);
    assert(tv4_outcome_rx_poll(&f.rx,60)==TV4_OUTCOME_ERROR); /* checked even at linger boundary */
    setup(&f,10,77); begin(&f);
    assert(tv4_outcome_tx_poll(&f.tx,9)==TV4_OUTCOME_ERROR && f.tx.context.error==TV4_OUTCOME_ERROR_CLOCK);
    setup(&f,10,77); begin(&f);
    assert(tv4_outcome_rx_abort(&f.rx,(tv4_update_id){77,43},10)==TV4_OUTCOME_REJECT_BINDING);
    assert(tv4_outcome_rx_abort(&f.rx,f.report.binding.image.id,10)==TV4_OUTCOME_OK);
    assert(tv4_outcome_rx_receive(&f.rx,&f.report,&f.ack,&f.delivery,10)==TV4_OUTCOME_REJECT_STATE);
    setup(&f,UINT32_MAX-5u,78); begin(&f);
    assert(tv4_outcome_tx_next(&f.tx,&f.packet,f.now)==TV4_OUTCOME_SENT);
    f.now+=10;
    assert(tv4_outcome_rx_receive(&f.rx,&f.packet,&f.ack,&f.delivery,f.now)==TV4_OUTCOME_DELIVERED);
    assert(tv4_outcome_tx_ack(&f.tx,&f.ack,f.now)==TV4_OUTCOME_OK);
}
static void config_and_canonical_codes(void)
{
    fixture f; setup(&f,10,77);
    tv4_outcome_limits bad=f.limits; bad.retry_limit=0; assert(!tv4_outcome_tx_init(&f.tx,bad,77,10));
    bad=f.limits; bad.ack_timeout_ms=bad.linger_ms; assert(!tv4_outcome_rx_init(&f.rx,bad,10));
    bad=f.limits; bad.total_timeout_ms=UINT32_MAX; assert(!tv4_outcome_rx_init(&f.rx,bad,10));
    assert(!tv4_outcome_tx_init(&f.tx,f.limits,0,10));
    assert(!tv4_update_outcome_valid(NULL));
    for (unsigned code=0;code<7;++code) for (unsigned reason=0;reason<20;++reason) {
        tv4_update_outcome r=f.report; r.code=(tv4_outcome_code)code; r.reason=(tv4_outcome_reason)reason;
        bool valid=tv4_update_outcome_valid(&r);
        if (valid) assert(code>=1 && code<=5 && (code!=1 || reason==0));
    }
    tv4_update_outcome empty={0}; assert(!tv4_update_outcome_valid(&empty));
    assert(!tv4_update_binding_equal(NULL,&f.report.binding) && !tv4_update_outcome_equal(NULL,&f.report));
    assert(zero(&empty,sizeof(empty)));
}
#define RUN(name) do { name(); puts("PASS " #name); } while(0)
int main(void)
{
    RUN(loss_duplicate_and_history); RUN(binding_and_ack_validation); RUN(conflicts_invalid_reports_and_forgery_boundary);
    RUN(deadlines_retries_and_late_begin); RUN(transport_reboot_abort_and_clock); RUN(config_and_canonical_codes);
    printf("PASS 6 outcome groups; report=%zu TX=%zu RX=%zu; no authentication/install/device access\n",
        sizeof(tv4_update_outcome),sizeof(tv4_outcome_tx),sizeof(tv4_outcome_rx)); return 0;
}
