#include "diagnostics.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
void diag_increment(uint32_t *value) { if(*value<UINT32_MAX)++*value; }
void diag_histogram_add(diag_histogram_t *h,uint32_t value) {
    unsigned bin=0;
    while(bin<16 && value>(UINT32_C(1)<<bin))++bin;
    if(h->count==UINT32_MAX)return;
    ++h->bins[bin];++h->count;if(value>h->maximum)h->maximum=value;
}
uint32_t diag_histogram_p95(const diag_histogram_t *h) {
    if(!h->count)return 0;
    uint32_t target=(uint32_t)(((uint64_t)h->count*95+99)/100),sum=0;
    for(unsigned i=0;i<17;i++) {
        sum+=h->bins[i];
        if(sum>=target) {
            uint32_t bound=i==16?h->maximum:(UINT32_C(1)<<i);
            return bound>h->maximum?h->maximum:bound;
        }
    }
    return h->maximum;
}
void diagnostics_init(diagnostics_t *d,uint32_t now_ms) {
    memset(d,0,sizeof(*d));d->started_ms=now_ms;
}
void diagnostics_render(diagnostics_t *d,uint32_t ms) {
    diag_histogram_add(&d->render,ms);diag_increment(&d->frames);
}
void diagnostics_event(diagnostics_t *d,uint32_t ms) { diag_histogram_add(&d->event,ms); }
void diagnostics_work(diagnostics_t *d,uint32_t us) {
    if(us>d->work_max)d->work_max=us;
    if(UINT64_MAX-d->work_total<us)d->work_total=UINT64_MAX;else d->work_total+=us;
}
bool diagnostics_window(diagnostics_t *d,uint32_t now_ms) {
    uint32_t elapsed=now_ms-d->started_ms;if(elapsed<1000)return false;
    diag_record_t *r=&d->record;r->uptime_ms=now_ms;r->window_ms=elapsed;
    uint64_t hz=(uint64_t)d->frames*1000/elapsed;
    r->refresh_hz=hz>UINT32_MAX?UINT32_MAX:(uint32_t)hz;
    r->render_samples=d->render.count;r->event_samples=d->event.count;
    r->render_max_ms=d->render.maximum;r->render_p95_ms=diag_histogram_p95(&d->render);
    r->event_max_ms=d->event.maximum;r->event_p95_ms=diag_histogram_p95(&d->event);
    r->owner_max_us=d->work_max;
    /* Sum is microseconds, elapsed is milliseconds; ratio is per-mille.
     * This is owner wall-time duty, NOT total CPU load or per-core utilization. */
    uint64_t busy=d->work_total/elapsed;r->owner_busy_permille=busy>1000?1000:(uint32_t)busy;
    memset(&d->render,0,sizeof(d->render));memset(&d->event,0,sizeof(d->event));
    d->frames=0;d->work_max=0;d->work_total=0;d->started_ms=now_ms;return true;
}
bool diagnostics_format(const diag_record_t *r,char *out,size_t size) {
    if(!out || !size)return false;
    int n=snprintf(out,size,
        "{\"schema\":1,\"platform\":\"%s\",\"uptime_ms\":%" PRIu32
        ",\"profile_rev12\":%s,\"render_samples\":%" PRIu32
        ",\"event_samples\":%" PRIu32 ",\"touch_age_ms\":%" PRIu32
        ",\"window_ms\":%" PRIu32 ",\"lvgl_refresh_hz\":%" PRIu32
        ",\"render_max_ms\":%" PRIu32 ",\"render_p95_upper_ms\":%" PRIu32
        ",\"event_max_ms\":%" PRIu32 ",\"event_p95_upper_ms\":%" PRIu32
        ",\"owner_max_us\":%" PRIu32 ",\"owner_busy_permille\":%" PRIu32
        ",\"resources_valid\":%s,\"internal_free\":%" PRIu32 ",\"internal_min\":%" PRIu32
        ",\"internal_largest\":%" PRIu32 ",\"psram_free\":%" PRIu32
        ",\"psram_min\":%" PRIu32 ",\"psram_total\":%" PRIu32
        ",\"owner_stack\":%" PRIu32 ",\"touch_stack\":%" PRIu32
        ",\"wifi_stack\":%" PRIu32 ",\"export_stack\":%" PRIu32
        ",\"queue_drops\":%" PRIu32 ",\"mock_can_timeouts\":%" PRIu32
        ",\"display_errors\":%" PRIu32 ",\"export_drops\":%" PRIu32
        ",\"wifi_attempts\":%" PRIu32 ",\"wifi_disconnects\":%" PRIu32
        ",\"wifi_ip_events\":%" PRIu32 ",\"wifi_queue_drops\":%" PRIu32
        ",\"touch_errors\":%" PRIu32 ",\"reset_reason\":%" PRIu32
        ",\"watchdog_boot\":%" PRIu32
        ",\"cpu_percent\":null,\"touch_to_display_ms\":null,\"rgb_scanout_hz\":null}",
        r->board?"esp32":"host",r->uptime_ms,r->rev12?"true":"false",r->render_samples,r->event_samples,r->touch_age_ms,r->window_ms,r->refresh_hz,
        r->render_max_ms,r->render_p95_ms,r->event_max_ms,r->event_p95_ms,
        r->owner_max_us,r->owner_busy_permille,r->resources_valid?"true":"false",
        r->internal_free,r->internal_min,r->internal_largest,r->psram_free,r->psram_min,r->psram_total,
        r->owner_stack,r->touch_stack,r->wifi_stack,r->export_stack,r->queue_drops,
        r->mock_can_timeouts,r->display_errors,r->export_drops,r->wifi_attempts,
        r->wifi_disconnects,r->wifi_ip_events,r->wifi_queue_drops,r->touch_errors,
        r->reset_reason,r->watchdog_boot);
    if(n<0 || (size_t)n>=size) { out[0]=0;return false; }return true;
}
