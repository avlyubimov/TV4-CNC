#include "diagnostics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    diagnostics_t d;diagnostics_init(&d,UINT32_MAX-499);
    for(unsigned i=0;i<100;i++) { diagnostics_render(&d,i<95?3:70);diagnostics_work(&d,1000); }
    diagnostics_event(&d,0);diagnostics_event(&d,7);
    diag_increment(&d.record.queue_drops);
    assert(!diagnostics_window(&d,499));
    assert(diagnostics_window(&d,500));
    assert(d.record.render_samples==100 && d.record.event_samples==2);
    assert(d.record.window_ms==1000 && d.record.refresh_hz==100);
    assert(d.record.render_max_ms==70 && d.record.render_p95_ms==4);
    assert(d.record.event_max_ms==7 && d.record.event_p95_ms==7);
    assert(d.record.owner_max_us==1000 && d.record.owner_busy_permille==100);
    assert(d.record.queue_drops==1);
    assert(diagnostics_window(&d,1500));
    assert(d.record.render_max_ms==0 && d.record.event_max_ms==0);
    assert(d.record.queue_drops==1); /* counters survive window reset */
    diag_histogram_t h={0};diag_histogram_add(&h,UINT32_MAX);
    assert(diag_histogram_p95(&h)==UINT32_MAX);
    uint32_t counter=UINT32_MAX;diag_increment(&counter);assert(counter==UINT32_MAX);
    char json[1536];memset(&d.record,0xff,sizeof(d.record));
    d.record.board=true;d.record.resources_valid=true;d.record.rev12=true;
    assert(diagnostics_format(&d.record,json,sizeof(json)));
    assert(strstr(json,"\"cpu_percent\":null"));
    assert(strstr(json,"\"touch_to_display_ms\":null"));
    assert(!strstr(json,"password") && !strstr(json,"ssid"));
    assert(json[0]=='{' && json[strlen(json)-1]=='}');
    char tiny[4]="abc";assert(!diagnostics_format(&d.record,tiny,sizeof(tiny)) && !tiny[0]);
    assert(!diagnostics_format(&d.record,NULL,0));
    puts("Diagnostics windows, wraparound, p95 bounds, saturation and bounded export PASS");
}
