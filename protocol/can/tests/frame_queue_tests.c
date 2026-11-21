#include "frame_queue.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static bool zero(const void *p,size_t n)
{const uint8_t *b=p;for(size_t i=0;i<n;++i)if(b[i]!=0)return false;return true;}
static tv4_fragment_frame sample(unsigned i)
{return (tv4_fragment_frame){.bytes={(uint8_t)(i+1u),0,8,0x80,1,2,3,4},.length=(uint8_t)(5u+i%4u)};}
static void init(tv4_frame_queue *q,unsigned capacity,uint32_t last,uint32_t now)
{assert(tv4_frame_queue_init(q,(tv4_frame_queue_limits){20,(uint8_t)capacity},7,11,last,now));}
static tv4_frame_ticket admit(tv4_frame_queue *q,unsigned value,uint32_t now)
{
    tv4_frame_ticket ticket;tv4_fragment_frame frame=sample(value);
    assert(tv4_frame_queue_admit(q,&frame,&ticket,11,now)==TV4_QUEUE_ADMITTED);
    return ticket;
}
static void fifo_and_tickets(void)
{
    unsigned admitted=0;
    for(unsigned capacity=1;capacity<=4;++capacity) {
        tv4_frame_queue q={0};uint32_t now=UINT32_MAX-2u;init(&q,capacity,7,now);
        assert(!tv4_frame_queue_init(&q,(tv4_frame_queue_limits){20,1},7,11,0,now));
        for(unsigned round=0;round<1024;++round) {
            tv4_frame_ticket tickets[4];
            for(unsigned i=0;i<capacity;++i)tickets[i]=admit(&q,i,now);
            tv4_frame_ticket rejected={.generation=99,.serial=99};tv4_fragment_frame frame=sample(0);uint32_t issued=q.last_issued;
            assert(tv4_frame_queue_admit(&q,&frame,&rejected,11,now)==TV4_QUEUE_BUSY && zero(&rejected,sizeof(rejected)) && q.last_issued==issued);
            assert(tv4_frame_queue_quiescent(&q,11,now)==TV4_QUEUE_BUSY);
            assert(tv4_frame_queue_clear(&q,11,now)==TV4_QUEUE_STATE);
            for(unsigned i=0;i<capacity;++i) {
                tv4_frame_delivery out,held;
                assert(tv4_frame_queue_claim(&q,&out,11,now)==TV4_QUEUE_FRAME);
                assert(out.ticket.generation==tickets[i].generation && out.ticket.serial==tickets[i].serial && out.route==7);
                tv4_fragment_frame expected=sample(i);
                assert(out.frame.length==expected.length && !memcmp(out.frame.bytes,expected.bytes,expected.length));
                for(unsigned j=out.frame.length;j<8;++j)assert(out.frame.bytes[j]==0);
                memset(&held,0xa5,sizeof(held));
                assert(tv4_frame_queue_claim(&q,&held,11,now)==TV4_QUEUE_HELD && zero(&held,sizeof(held)));
                tv4_frame_ticket stale=out.ticket;++stale.serial;
                assert(tv4_frame_queue_complete(&q,stale,TV4_FRAME_SENT_LOCAL,now)==TV4_QUEUE_STALE && q.flight==out.ticket.serial);
                stale=out.ticket;++stale.route;
                assert(tv4_frame_queue_complete(&q,stale,TV4_FRAME_SENT_LOCAL,now)==TV4_QUEUE_STALE && q.flight==out.ticket.serial);
                stale=out.ticket;++stale.generation;
                assert(tv4_frame_queue_complete(&q,stale,TV4_FRAME_SENT_LOCAL,now)==TV4_QUEUE_STALE && q.flight==out.ticket.serial);
                assert(tv4_frame_queue_complete(&q,out.ticket,TV4_FRAME_SENT_LOCAL,now)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED);
                assert(tv4_frame_queue_complete(&q,out.ticket,TV4_FRAME_SENT_LOCAL,now)==TV4_QUEUE_STALE);
                ++now;++admitted;
            }
            assert(q.count==0 && q.flight==0 && zero(q.entries,sizeof(q.entries)));
            assert(tv4_frame_queue_quiescent(&q,11,now)==TV4_QUEUE_OK);
            assert(q.last_issued==7u+(round+1u)*capacity);
        }
    }
    printf("PASS capacities1..4, 4096 FIFO cycles/%u owned frames; full/claim/duplicate/stale tickets, retained serials and wrap clock\n",admitted);
}
static void held_faults(void)
{
    for(unsigned which=0;which<7;++which) {
        tv4_frame_queue q={0};uint32_t now=100;init(&q,4,0,now);
        tv4_frame_ticket first=admit(&q,0,now);(void)admit(&q,1,now);(void)admit(&q,2,now);
        tv4_frame_delivery claimed;assert(tv4_frame_queue_claim(&q,&claimed,11,now)==TV4_QUEUE_FRAME);
        uint32_t admitted_at=q.entries[q.head].admitted;
        switch(which) {
        case 0:now+=20;assert(tv4_frame_queue_poll(&q,11,now)==TV4_QUEUE_ERROR && q.error==TV4_QUEUE_TIMEOUT);break;
        case 1:assert(tv4_frame_queue_cancel(&q,11,now)==TV4_QUEUE_HELD);break;
        case 2:assert(tv4_frame_queue_complete(&q,first,TV4_FRAME_OWNERSHIP_UNCERTAIN,now)==TV4_QUEUE_HELD && q.error==TV4_QUEUE_UNCERTAIN);break;
        case 3:assert(tv4_frame_queue_poll(&q,12,now)==TV4_QUEUE_ERROR && q.error==TV4_QUEUE_CONTEXT);break;
        case 4:--now;assert(tv4_frame_queue_poll(&q,11,now)==TV4_QUEUE_ERROR && q.error==TV4_QUEUE_CLOCK);break;
        case 5:now+=20;assert(tv4_frame_queue_complete(&q,first,TV4_FRAME_OWNERSHIP_UNCERTAIN,now)==TV4_QUEUE_HELD && q.error==TV4_QUEUE_TIMEOUT);break;
        default:assert(tv4_frame_queue_complete(&q,first,(tv4_frame_driver_result)99,now)==TV4_QUEUE_ARGUMENT);assert(tv4_frame_queue_cancel(&q,11,now)==TV4_QUEUE_HELD);break;
        }
        assert(q.flight==first.serial && q.entries[q.head].admitted==admitted_at);
        assert(tv4_frame_queue_cancel(&q,11,now)==TV4_QUEUE_HELD && q.count==1);
        assert(tv4_frame_queue_clear(&q,11,now)!=TV4_QUEUE_OK);
        tv4_frame_delivery no;memset(&no,0xa5,sizeof(no));
        assert(tv4_frame_queue_claim(&q,&no,11,now)==TV4_QUEUE_ERROR && zero(&no,sizeof(no)));
        assert(tv4_frame_queue_complete(&q,first,TV4_FRAME_SENT_LOCAL,now)==TV4_QUEUE_RECLAIMED_FAILED);
        assert(q.count==0 && q.flight==0 && zero(q.entries,sizeof(q.entries)));
        assert(tv4_frame_queue_quiescent(&q,11,now)==TV4_QUEUE_ERROR);
        if(which==3 || which==4)assert(tv4_frame_queue_clear(&q,11,now)==TV4_QUEUE_ERROR);
        else {
            assert(tv4_frame_queue_clear(&q,11,now)==TV4_QUEUE_OK && q.last_issued==3);
            tv4_frame_ticket next=admit(&q,3,now);assert(next.serial==4);
            tv4_frame_delivery actual;assert(tv4_frame_queue_claim(&q,&actual,11,now)==TV4_QUEUE_FRAME);
            assert(tv4_frame_queue_complete(&q,first,TV4_FRAME_SENT_LOCAL,now)==TV4_QUEUE_STALE && q.flight==next.serial);
            assert(tv4_frame_queue_complete(&q,next,TV4_FRAME_SENT_LOCAL,now)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED && q.count==0);
        }
    }
    for(unsigned result=TV4_FRAME_FAILED_LOCAL;result<=TV4_FRAME_CANCELLED_LOCAL;++result) {
        tv4_frame_queue q={0};init(&q,1,0,0);tv4_frame_ticket t=admit(&q,0,0);tv4_frame_delivery d;
        assert(tv4_frame_queue_claim(&q,&d,11,0)==TV4_QUEUE_FRAME);
        assert(tv4_frame_queue_complete(&q,t,(tv4_frame_driver_result)result,0)==TV4_QUEUE_RECLAIMED_FAILED && q.stopped && q.count==0);
    }
    puts("PASS timeout/cancel/uncertain/context/clock preserves actual claim; exact late terminal return reclaims without success/revival");
}
static void boundaries(void)
{
    for(unsigned which=0;which<4;++which) {
        tv4_frame_queue q={0};tv4_frame_queue_limits l={20,1};
        switch(which){case 0:l.capacity=0;break;case 1:l.capacity=5;break;case 2:l.residency_ms=0;break;default:l.residency_ms=UINT32_MAX;break;}
        assert(!tv4_frame_queue_init(&q,l,7,11,0,0) && !q.configured);
    }
    tv4_frame_queue q={0};init(&q,1,UINT32_MAX-1u,0);tv4_fragment_frame f=sample(0);tv4_frame_ticket t;
    assert(tv4_frame_queue_admit(&q,&f,&t,11,0)==TV4_QUEUE_ADMITTED && t.serial==UINT32_MAX);
    tv4_frame_delivery d;assert(tv4_frame_queue_claim(&q,&d,11,0)==TV4_QUEUE_FRAME);
    assert(tv4_frame_queue_complete(&q,t,TV4_FRAME_SENT_LOCAL,0)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED);
    assert(tv4_frame_queue_admit(&q,&f,&t,11,0)==TV4_QUEUE_ERROR && zero(&t,sizeof(t)) && q.error==TV4_QUEUE_EXHAUSTED);
    assert(tv4_frame_queue_cancel(&q,11,0)==TV4_QUEUE_OK && tv4_frame_queue_clear(&q,11,0)==TV4_QUEUE_ERROR);
    tv4_frame_queue bad={0};init(&bad,1,0,0);
    for(unsigned dlc=0;dlc<=255;++dlc)if(dlc<5 || dlc>8) {
        f.length=(uint8_t)dlc;t=(tv4_frame_ticket){.generation=99,.serial=99};
        assert(tv4_frame_queue_admit(&bad,&f,&t,11,0)==TV4_QUEUE_ARGUMENT && zero(&t,sizeof(t)) && bad.last_issued==0);
    }
    f=sample(0);(void)admit(&bad,0,0);assert(tv4_frame_queue_poll(&bad,11,20)==TV4_QUEUE_ERROR);
    assert(tv4_frame_queue_cancel(&bad,11,20)==TV4_QUEUE_OK && bad.count==0);
    assert(tv4_frame_queue_clear(&bad,11,20)==TV4_QUEUE_OK && bad.last_issued==1);
    tv4_frame_queue a={0},b={0};init(&a,1,0,0);init(&b,1,0,0);
    (void)admit(&a,0,0);(void)admit(&b,0,0);tv4_frame_delivery da,db;
    assert(tv4_frame_queue_claim(&a,&da,11,0)==TV4_QUEUE_FRAME && tv4_frame_queue_claim(&b,&db,11,0)==TV4_QUEUE_FRAME);
    assert(da.ticket.serial==db.ticket.serial && da.ticket.generation==db.ticket.generation && da.ticket.route==db.ticket.route);
    assert(tv4_frame_queue_complete(&b,da.ticket,TV4_FRAME_SENT_LOCAL,0)==TV4_QUEUE_STALE && b.flight==db.ticket.serial);
    assert(tv4_frame_queue_complete(&a,db.ticket,TV4_FRAME_SENT_LOCAL,0)==TV4_QUEUE_STALE && a.flight==da.ticket.serial);
    assert(tv4_frame_queue_complete(&a,da.ticket,TV4_FRAME_SENT_LOCAL,0)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED);
    assert(tv4_frame_queue_complete(&b,db.ticket,TV4_FRAME_SENT_LOCAL,0)==TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED);
    puts("PASS cross-queue identical generation/route/serial cannot free a different actual owner; terminal cleanup explicit");
    puts("PASS invalid config/DLC256/exhaustion/no wrap, timeout before claim, explicit safe queued cancellation");
}
int main(void)
{
    fifo_and_tickets();held_faults();boundaries();
    printf("Host storage only: frame queue=%zu driver snapshot=%zu; no MCU RAM/physical queue approval\n",sizeof(tv4_frame_queue),sizeof(tv4_frame_delivery));return 0;
}
