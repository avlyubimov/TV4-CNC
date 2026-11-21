#include "completion_mailbox.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
static bool zero(const void *p,size_t n)
{const uint8_t *b=p;for(size_t i=0;i<n;++i)if(b[i]!=0)return false;return true;}
static tv4_driver_event event(const tv4_frame_queue *q,uint32_t serial)
{return (tv4_driver_event){.ticket={.generation=q->generation,.serial=serial,.route=q->route,.owner=q},.result=(tv4_frame_driver_result)(1u+serial%4u)};}
static void bounded_fifo(void)
{
    unsigned records=0;
    for(unsigned capacity=1;capacity<=4;++capacity) {
        tv4_frame_queue q={0};assert(tv4_frame_queue_init(&q,(tv4_frame_queue_limits){40,1},7,11,0,0));
        tv4_completion_mailbox m={0};assert(tv4_completion_mailbox_init(&m,capacity,&q));
        for(unsigned cycle=0;cycle<4096;++cycle) {
            assert(tv4_completion_mailbox_empty(&m)==TV4_QUEUE_OK);
            for(unsigned n=1;n<=capacity;++n) {
                tv4_driver_event e=event(&q,cycle*capacity+n);
                assert(tv4_completion_mailbox_post(&m,&e)==TV4_QUEUE_ADMITTED);memset(&e,0,sizeof(e));
            }
            tv4_driver_event e=event(&q,1);unsigned head=atomic_load(&m.head),tail=atomic_load(&m.tail);
            assert(tv4_completion_mailbox_post(&m,&e)==TV4_QUEUE_BUSY && atomic_load(&m.head)==head && atomic_load(&m.tail)==tail);
            for(unsigned n=1;n<=capacity;++n) {
                assert(tv4_completion_mailbox_take(&m,&e)==TV4_QUEUE_FRAME);
                assert(e.ticket.serial==cycle*capacity+n && e.ticket.owner==&q && e.ticket.generation==11 && e.ticket.route==7);
                assert(e.result==(tv4_frame_driver_result)(1u+e.ticket.serial%4u));++records;
            }
            memset(&e,0xa5,sizeof(e));assert(tv4_completion_mailbox_take(&m,&e)==TV4_QUEUE_WAIT && zero(&e,sizeof(e)));
        }
        /* Inspection only with both actors stopped: every freed slot wiped. */
        assert(zero(m.entries,sizeof(m.entries)) && q.last_issued==0 && q.flight==0);
    }
    printf("PASS capacities1..4,16384 FIFO cycles/%u owned reports, non-power-of-two ring wrap/full retry/no ledger mutation\n",records);
}
static void invalid(void)
{
    tv4_frame_queue q={0},other={0};assert(tv4_frame_queue_init(&q,(tv4_frame_queue_limits){40,1},7,11,0,0));
    assert(tv4_frame_queue_init(&other,(tv4_frame_queue_limits){40,1},7,11,0,0));
    tv4_completion_mailbox m={0};assert(!tv4_completion_mailbox_init(&m,0,&q) && !m.configured);
    assert(!tv4_completion_mailbox_init(&m,5,&q));assert(tv4_completion_mailbox_init(&m,3,&q));
    assert(!tv4_completion_mailbox_init(&m,3,&q));
    tv4_driver_event e=event(&other,1);assert(tv4_completion_mailbox_post(&m,&e)==TV4_QUEUE_STALE);
    for(unsigned i=0;i<4;++i) {
        e=event(&q,1);if(i==0)++e.ticket.route;else if(i==1)++e.ticket.generation;else if(i==2)e.ticket.serial=0;else e.result=(tv4_frame_driver_result)256;
        assert(tv4_completion_mailbox_post(&m,&e)==(i==3?TV4_QUEUE_ARGUMENT:TV4_QUEUE_STALE));
    }
    assert(tv4_completion_mailbox_empty(&m)==TV4_QUEUE_OK);
    assert(tv4_completion_mailbox_post(&m,NULL)==TV4_QUEUE_ARGUMENT);
    memset(&e,0xa5,sizeof(e));assert(tv4_completion_mailbox_take(NULL,&e)==TV4_QUEUE_ARGUMENT && zero(&e,sizeof(e)));
}
typedef struct {tv4_completion_mailbox *m;const tv4_frame_queue *q;uint32_t count;unsigned busy;} thread_context;
static void *producer(void *arg)
{
    thread_context *c=arg;
    for(uint32_t n=1;n<=c->count;++n) {
        tv4_driver_event e=event(c->q,n);tv4_frame_queue_result r;
        while((r=tv4_completion_mailbox_post(c->m,&e))==TV4_QUEUE_BUSY){++c->busy;sched_yield();}
        assert(r==TV4_QUEUE_ADMITTED);memset(&e,0,sizeof(e));
    }
    return NULL;
}
static void two_threads(void)
{
    unsigned busy=0;
    for(unsigned capacity=1;capacity<=4;++capacity) {
        tv4_frame_queue q={0};assert(tv4_frame_queue_init(&q,(tv4_frame_queue_limits){40,1},7,11,0,0));
        tv4_completion_mailbox m={0};assert(tv4_completion_mailbox_init(&m,capacity,&q));
        thread_context c={&m,&q,300000,0};pthread_t thread;assert(pthread_create(&thread,NULL,producer,&c)==0);
        for(uint32_t n=1;n<=c.count;++n) {
            tv4_driver_event e;tv4_frame_queue_result r;
            while((r=tv4_completion_mailbox_take(&m,&e))==TV4_QUEUE_WAIT)sched_yield();
            assert(r==TV4_QUEUE_FRAME && e.ticket.owner==&q && e.ticket.serial==n && e.ticket.generation==11 && e.ticket.route==7);
            assert(e.result==(tv4_frame_driver_result)(1u+n%4u));
        }
        assert(pthread_join(thread,NULL)==0 && tv4_completion_mailbox_empty(&m)==TV4_QUEUE_OK);busy+=c.busy;
        assert(zero(m.entries,sizeof(m.entries)) && q.last_issued==0);
    }
    printf("PASS two real pthread actors/1200000 ordered reports/capacities1..4, producer retries=%u; no claim/driver/auth authority\n",busy);
}
int main(int argc,char **argv)
{
    if(argc==2 && strcmp(argv[1],"--threads")==0){two_threads();return 0;}
    invalid();bounded_fifo();printf("Host storage only: atomic mailbox=%zu; atomic_uint lock-free macro=%d, no MCU ISR/RTOS approval\n",sizeof(tv4_completion_mailbox),ATOMIC_INT_LOCK_FREE);return 0;
}
