#include "timed_download.h"
typedef struct {
    const tv4_http_ops *io;tv4_time_anchor *anchor;
    tv4_time_reader reader;void *clock_context;bool failed;
} timed;
static bool check(timed *t) {
    tv4_time_sample now={0};
    if(t->reader(t->clock_context,&now)!=TV4_TIME_OK || tv4_time_check(t->anchor,&now)!=TV4_TIME_OK) {
        t->failed=true;tv4_time_forget(t->anchor);return false;
    }
    return true;
}
static bool open_io(void *context,const char *url,uint32_t timeout,tv4_http_response *response) {
    timed *t=context;if(!check(t))return false;
    bool opened=t->io->open(t->io->context,url,timeout,response);
    return check(t) && opened;
}
static int read_io(void *context,uint8_t *bytes,size_t length) {
    timed *t=context;if(!check(t))return -1;
    int count=t->io->read(t->io->context,bytes,length);
    return check(t)?count:-1;
}
static bool complete_io(void *context) {
    timed *t=context;if(!check(t))return false;
    bool complete=t->io->complete(t->io->context);return check(t) && complete;
}
static void close_io(void *context) {timed *t=context;t->io->close(t->io->context);}
static uint32_t now_io(void *context) {timed *t=context;return t->io->now_ms(t->io->context);}
static bool cancel_io(void *context) {timed *t=context;return t->io->cancelled && t->io->cancelled(t->io->context);}
tv4_http_result tv4_http_fetch_timed(const tv4_http_request *r,const tv4_http_ops *io,
    tv4_time_anchor *anchor,tv4_time_reader reader,void *clock_context,tv4_http_workspace *w,
    tv4_http_consume consume,void *consumer,tv4_http_report *report) {
    if(report)*report=(tv4_http_report){0};
    if(!r || !io || !reader || !w || !consume || !report || !io->open || !io->read ||
       !io->complete || !io->close || !io->now_ms)return TV4_HTTP_ARGUMENT;
    timed t={io,anchor,reader,clock_context,false};
    if(!r->trusted_time || !check(&t))return TV4_HTTP_UNTRUSTED_TIME;
    tv4_http_ops guarded={&t,open_io,read_io,complete_io,close_io,now_io,cancel_io};
    tv4_http_result result=tv4_http_fetch(r,&guarded,w,consume,consumer,report);
    /* Includes clock changes in consumer/cleanup, even after complete body. */
    if(!check(&t) || t.failed)return TV4_HTTP_UNTRUSTED_TIME;
    return result;
}
