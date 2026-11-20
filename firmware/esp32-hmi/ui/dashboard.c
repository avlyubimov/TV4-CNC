#include "dashboard.h"
#include "model.h"
#include "strings.h"
#include "tools_view.h"
#include "wifi_view.h"
#include "firmware_view.h"
#include <stdint.h>
#include <stdio.h>
#ifdef ESP_PLATFORM
#include "esp_system.h"
#endif
LV_FONT_DECLARE(tv4_ru_20);
LV_FONT_DECLARE(tv4_digits_36);
LV_FONT_DECLARE(tv4_digits_32);
static model_t model, rendered;
static diagnostics_t diagnostics;
diagnostics_t *dashboard_diagnostics(void) { return &diagnostics; }
static bool view_valid;
static dashboard_view_t view, pending_view;
static bool view_requested;
static lv_obj_t *home_objects[96], *navigation[6], *diag_panel, *log_panel, *tools_panel, *wifi_panel;
static lv_obj_t *settings_panel;
static firmware_view_t firmware_view;
static firmware_info_t firmware_info;
void dashboard_set_firmware_info(const firmware_info_t *info) {
    if(info)firmware_info_set(&firmware_info,info->build,sizeof(info->build),
        info->sdk,sizeof(info->sdk),info->source,info->profile);
    else firmware_info_reset(&firmware_info);
    firmware_view_update(&firmware_view,&firmware_info);
}
static lv_obj_t *home_tool_id,*home_tool_name,*home_tool_code,*home_tool_radius;
static unsigned home_count, log_page;
static lv_obj_t *diag_state, *diag_fault, *diag_channel, *diag_frames, *diag_heap, *diag_latency;
static lv_obj_t *log_summary, *log_page_label, *log_time[8], *log_event[8], *log_reason[8];
static uint32_t metrics_revision, rendered_metrics, measured_fps, free_bytes;
static bool metrics_valid;
static lv_obj_t *resource_panel,*diag_left,*diag_right,*resource_lines[5];
static bool resource_view,resource_requested;
static void resource_request(lv_event_t *ev) { (void)ev;resource_requested=true; }

static lv_obj_t *override_slider, *override_value;
static bool override_pending;
static unsigned pending_override;
static lv_obj_t *frame_control,*coordinate_frame,*zero_targets[2],*zero_all_button,*zero_dialog;
static bool zero_requested,zero_close;
static event_t zero_action;
static lv_obj_t *status, *x_value, *z_value;
static lv_obj_t *increments[4], *rpm_value, *speed_value, *rpm_set_value, *feed_rev_value, *rpm_arc;
LV_FONT_DECLARE(tv4_ru_14);
LV_FONT_DECLARE(tv4_ru_12);
LV_FONT_DECLARE(tv4_button_12);
LV_FONT_DECLARE(tv4_bold_24);
LV_IMAGE_DECLARE(tv4_lathe);
LV_IMAGE_DECLARE(tv4_icon_home);
LV_IMAGE_DECLARE(tv4_icon_code);
LV_IMAGE_DECLARE(tv4_icon_tool);
LV_IMAGE_DECLARE(tv4_icon_sliders);
LV_IMAGE_DECLARE(tv4_icon_pulse);
LV_IMAGE_DECLARE(tv4_icon_file);
LV_IMAGE_DECLARE(tv4_icon_hand);
LV_IMAGE_DECLARE(tv4_icon_layers);
LV_IMAGE_DECLARE(tv4_icon_target);
LV_IMAGE_DECLARE(tv4_icon_reset);
LV_IMAGE_DECLARE(tv4_icon_wifi);
LV_IMAGE_DECLARE(tv4_icon_gear);
LV_IMAGE_DECLARE(tv4_icon_gauge);
LV_IMAGE_DECLARE(tv4_icon_spindle);
LV_IMAGE_DECLARE(tv4_icon_wave);
LV_IMAGE_DECLARE(tv4_icon_thermo);
LV_IMAGE_DECLARE(tv4_icon_bolt);
LV_IMAGE_DECLARE(tv4_icon_check);
LV_IMAGE_DECLARE(tv4_icon_alert);
LV_IMAGE_DECLARE(tv4_icon_insert);
/* Only the LVGL owner consumes this bounded queue; callbacks never perform I/O. */
static event_t queue[8];
static unsigned read_pos, write_pos, used;
static event_t pending_fault;
static uint32_t dropped_events;
static uint32_t event_ms[8], touch_latency_ms;
static uint32_t render_start_ms, render_ms, frames, log_ms;
static void render_event(lv_event_t *ev) {
    if(lv_event_get_code(ev)==LV_EVENT_REFR_START) render_start_ms=lv_tick_get();
    if(lv_event_get_code(ev)==LV_EVENT_REFR_READY) {
        render_ms=lv_tick_elaps(render_start_ms); frames++;
        diagnostics_render(&diagnostics,render_ms);
    }
}
bool dashboard_post_event(event_t event) {
    if(event<EV_STEP || event>EV_ZERO_WORK_ALL) { pending_fault=EV_INVALID_FAULT; dropped_events++; return false; }
    if(used==8) { pending_fault=EV_QUEUE_FAULT; dropped_events++; diag_increment(&diagnostics.record.queue_drops); return false; }
    queue[write_pos]=event; event_ms[write_pos]=lv_tick_get();
    write_pos=(write_pos+1)%8; used++; return true;
}
/* Coalesce drag events; callbacks only capture the requested value. */
static void override_changed(lv_event_t *ev) {
    pending_override=(unsigned)lv_slider_get_value(lv_event_get_target(ev));
    override_pending=true;
}
static void request_zero(lv_event_t *ev) {
    zero_action=(event_t)(uintptr_t)lv_event_get_user_data(ev);zero_requested=true;
}
static void zero_decision(lv_event_t *ev) {
    if((uintptr_t)lv_event_get_user_data(ev))dashboard_post_event(zero_action);
    zero_close=true;
}
static void click(lv_event_t *ev) {
    dashboard_post_event((event_t)(uintptr_t)lv_event_get_user_data(ev));
}
static lv_obj_t *label(lv_obj_t *parent, const char *text, int x,int y,int w) {
    lv_obj_t *o=lv_label_create(parent); lv_label_set_text(o,text);
    lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w); return o;
}
static lv_obj_t *panel(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color) {
    lv_obj_t *o=lv_obj_create(parent); lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_grad_color(o,lv_color_hex(0x060e12),0);
    lv_obj_set_style_bg_grad_dir(o,LV_GRAD_DIR_VER,0);
    lv_obj_set_style_radius(o,8,0);
    lv_obj_set_style_border_width(o,1,0);
    lv_obj_set_style_border_color(o,lv_color_hex(0x344752),0);
    lv_obj_set_style_shadow_color(o,lv_color_hex(0x000000),0);
    lv_obj_set_style_shadow_width(o,10,0);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE); return o;
}
static void line(lv_obj_t *p,int x,int y,int w,int h) {
    lv_obj_t *o=panel(p,x,y,w,h,0x344650);
    lv_obj_set_style_radius(o,0,0); lv_obj_set_style_shadow_width(o,0,0);
}
static void muted(lv_obj_t *o) { lv_obj_set_style_text_color(o,lv_color_hex(0xc1d6e8),0); }
static void icon(lv_obj_t *p,const lv_image_dsc_t *src,int x,int y,int size) {
    lv_obj_t *o=lv_image_create(p); lv_image_set_src(o,src);
    lv_obj_set_pos(o,x,y); lv_image_set_scale(o,size*256/24);
    lv_image_set_pivot(o,0,0);
}
static lv_obj_t *button(lv_obj_t *root,string_id id,int e,int x,int y,int w,int h,bool active) {
    lv_obj_t *b=lv_button_create(root); lv_obj_remove_style_all(b);
    lv_obj_set_pos(b,x,y); lv_obj_set_size(b,w,h);
    lv_obj_set_style_radius(b,7,0); lv_obj_set_style_bg_opa(b,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(b,lv_color_hex(e==EV_STOP?0xfc193d:active?0x009fff:0x354451),0);
    lv_obj_set_style_bg_grad_color(b,lv_color_hex(e==EV_STOP?0xa50017:active?0x0052bb:0x111c25),0);
    lv_obj_set_style_bg_grad_dir(b,LV_GRAD_DIR_VER,0);
    lv_obj_set_style_border_width(b,1,0);
    lv_obj_set_style_border_color(b,lv_color_hex(active?0x27d3ff:0x657d91),0);
    lv_obj_set_style_shadow_color(b,lv_color_hex(active?0x009cff:0x000000),0);
    lv_obj_set_style_shadow_width(b,active?12:(h<=34?8:13),0);
    lv_obj_set_style_shadow_offset_y(b,active?0:(h<=34?2:5),0);
    lv_obj_set_style_shadow_opa(b,active?LV_OPA_50:LV_OPA_90,0);
    lv_obj_set_style_bg_color(b,lv_color_hex(0x1486b0),LV_STATE_PRESSED);
    lv_obj_set_style_text_color(b,lv_color_hex(0xffffff),0);
    lv_obj_set_style_text_font(b,&tv4_button_12,0);
    lv_obj_set_style_shadow_width(b,3,LV_STATE_PRESSED);
    lv_obj_set_style_shadow_offset_y(b,1,LV_STATE_PRESSED);
    lv_obj_t *l=lv_label_create(b); lv_label_set_text(l,tr(0,id)); lv_obj_center(l);
    /* Thin inset highlight gives the bezel a lit top edge without a bitmap. */
    lv_obj_t *highlight=lv_obj_create(b); lv_obj_remove_style_all(highlight);
    lv_obj_set_pos(highlight,5,1); lv_obj_set_size(highlight,w-10,1);
    lv_obj_set_style_bg_color(highlight,lv_color_hex(active?0x8de6ff:0xb5c8d9),0);
    lv_obj_set_style_bg_opa(highlight,active?LV_OPA_70:LV_OPA_30,0);
    lv_obj_remove_flag(highlight,LV_OBJ_FLAG_CLICKABLE);
    if(e>=0) lv_obj_add_event_cb(b,click,LV_EVENT_CLICKED,(void *)(uintptr_t)e);
    else lv_obj_remove_flag(b,LV_OBJ_FLAG_CLICKABLE);
    return b;
}
static lv_obj_t *icon_button(lv_obj_t *p,string_id id,const lv_image_dsc_t *img,int x,int y,int w,int h,int e) {
    lv_obj_t *b=button(p,id,e,x,y,w,h,false);
    lv_obj_t *t=lv_obj_get_child(b,0); lv_obj_align(t,LV_ALIGN_BOTTOM_MID,0,-4);
    icon(b,img,w/2-10,5,20);return b;
}
static void update_zero_dialog(void) {
    if(zero_close || view!=VIEW_HOME || model.mode!=SIMULATION || model.connected) {
        if(zero_dialog) { lv_obj_delete(zero_dialog);zero_dialog=NULL; }
        zero_requested=false;zero_close=false;return;
    }
    if(!zero_requested || zero_dialog)return;
    zero_requested=false;
    zero_dialog=panel(lv_layer_top(),89,56,705,347,0x101f2d);
    lv_obj_set_style_text_font(zero_dialog,&tv4_ru_12,0);
    lv_obj_set_style_text_color(zero_dialog,lv_color_hex(0xf1f7ff),0);
    string_id id=zero_action==EV_ZERO_WORK_X?S_ZERO_WORK_X:zero_action==EV_ZERO_WORK_Z?S_ZERO_WORK_Z:S_ZERO_WORK_ALL;
    lv_obj_t *title=label(zero_dialog,tr(NULL,id),24,80,657);
    lv_obj_set_style_text_font(title,&tv4_ru_20,0);lv_obj_set_style_text_align(title,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_t *note=label(zero_dialog,tr(NULL,S_WORK_ZERO_NOTE),24,140,657);
    lv_obj_set_style_text_align(note,LV_TEXT_ALIGN_CENTER,0);
    string_id ids[]={S_CANCEL,S_ACCEPT};
    for(unsigned i=0;i<2;i++) {
        lv_obj_t *b=button(zero_dialog,ids[i],-1,105+(int)i*275,230,220,44,i==1);
        lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(b,zero_decision,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
    }
}
static const char *fault_text(fault_t fault) {
    static const string_id ids[]={S_FAULT_NONE,S_FAULT_EMERGENCY,S_FAULT_PROTOCOL,S_FAULT_QUEUE,S_FAULT_TOUCH,S_FAULT_DISPLAY,S_FAULT_INVALID};
    return tr(0,(unsigned)fault<sizeof(ids)/sizeof(*ids)?ids[fault]:S_FAULT_INVALID);
}
static const char *state_text(hmi_mode_t mode) {
    switch(mode) {
    case DISCONNECTED:return tr(0,S_DISCONNECTED);
    case READY:return tr(0,S_READY);
    case HOLD:return tr(0,S_PAUSE);
    case FAULT:return tr(0,S_FAULT);
    default:return tr(0,S_SIM);
    }
}
static void nav_event(lv_event_t *event) {
    dashboard_set_view((dashboard_view_t)(uintptr_t)lv_event_get_user_data(event));
}
bool dashboard_set_view(dashboard_view_t requested) {
    if(requested<VIEW_HOME || requested>VIEW_FIRMWARE) return false;
    pending_view=requested; view_requested=true; return true;
}
dashboard_view_t dashboard_get_view(void) { return view; }
static void create_settings(lv_obj_t *root) {
    settings_panel=panel(root,89,56,705,347,0x14212b);
    lv_obj_t *title=label(settings_panel,tr(NULL,S_SETTINGS),18,14,665);
    lv_obj_set_style_text_font(title,&tv4_ru_20,0);
    muted(label(settings_panel,tr(NULL,S_SETTINGS_HINT),18,49,665));
    const string_id names[]={S_SETTINGS_WIFI,S_FW_TITLE};
    const dashboard_view_t targets[]={VIEW_WIFI,VIEW_FIRMWARE};
    const lv_image_dsc_t *icons[]={&tv4_icon_wifi,&tv4_icon_layers};
    for(unsigned i=0;i<2;i++) {
        lv_obj_t *b=button(settings_panel,names[i],-1,18,100+(int)i*90,669,66,false);
        lv_obj_t *caption=lv_obj_get_child(b,0);
        lv_obj_set_style_text_font(caption,&tv4_ru_14,0);
        lv_obj_align(caption,LV_ALIGN_LEFT_MID,56,0);
        icon(b,icons[i],20,22,22);
        lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b,nav_event,LV_EVENT_CLICKED,(void *)(uintptr_t)targets[i]);
    }
}
static void page_event(lv_event_t *event) {
    if((uintptr_t)lv_event_get_user_data(event)) log_page++;
    else if(log_page) log_page--;
    view_valid=false;
}
static void apply_view(void) {
    if(!view_requested) return;
    view=pending_view; view_requested=false; view_valid=false;
    for(unsigned i=0;i<home_count;i++) {
        if(view==VIEW_HOME) lv_obj_remove_flag(home_objects[i],LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(home_objects[i],LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(diag_panel,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(log_panel,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(tools_panel,LV_OBJ_FLAG_HIDDEN);
    if(view==VIEW_DIAGNOSTICS) lv_obj_remove_flag(diag_panel,LV_OBJ_FLAG_HIDDEN);
    if(view==VIEW_LOG) lv_obj_remove_flag(log_panel,LV_OBJ_FLAG_HIDDEN);
    if(view==VIEW_TOOLS) lv_obj_remove_flag(tools_panel,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wifi_panel,LV_OBJ_FLAG_HIDDEN);
    if(view==VIEW_WIFI) { lv_obj_remove_flag(wifi_panel,LV_OBJ_FLAG_HIDDEN);wifi_view_show(); }
    else wifi_view_hide();
    if(view!=VIEW_SETTINGS && settings_panel) { lv_obj_delete(settings_panel);settings_panel=NULL; }
    if(view!=VIEW_FIRMWARE)firmware_view_destroy(&firmware_view);
    if(view==VIEW_SETTINGS && !settings_panel)create_settings(lv_screen_active());
    if(view==VIEW_FIRMWARE) {
        firmware_view_create(&firmware_view,lv_screen_active(),nav_event,(void *)(uintptr_t)VIEW_SETTINGS);
        firmware_view_update(&firmware_view,&firmware_info);
    }
    for(unsigned i=0;i<6;i++) {
        bool selected=(i==0 && view==VIEW_HOME)||(i==4 && view==VIEW_DIAGNOSTICS)||(i==5 && view==VIEW_LOG)||(i==2 && view==VIEW_TOOLS)||
            (i==3 && (view==VIEW_WIFI || view==VIEW_SETTINGS || view==VIEW_FIRMWARE));
        lv_obj_set_style_bg_opa(navigation[i],selected?LV_OPA_COVER:LV_OPA_TRANSP,0);
        lv_obj_set_style_border_opa(navigation[i],selected?LV_OPA_COVER:LV_OPA_TRANSP,0);
        lv_obj_set_style_shadow_opa(navigation[i],selected?LV_OPA_50:LV_OPA_TRANSP,0);
    }
}
static void create_views(lv_obj_t *root) {
    diag_panel=panel(root,89,56,705,347,0x14212b);
    lv_obj_t *title=label(diag_panel,tr(0,S_DIAG),14,10,670); lv_obj_set_style_text_font(title,&tv4_ru_20,0);
    muted(label(diag_panel,tr(0,S_DIAG_SUB),14,40,330));
#ifdef ESP_PLATFORM
    muted(label(diag_panel,tr(0,S_DEVICE_PLATFORM),356,40,335));
#else
    muted(label(diag_panel,tr(0,S_HOST_PLATFORM),356,40,335));
#endif
    lv_obj_t *toggle=button(diag_panel,S_RESOURCES,-1,546,5,145,32,false);
    lv_obj_add_event_cb(toggle,resource_request,LV_EVENT_CLICKED,NULL);
    resource_panel=panel(diag_panel,14,68,677,115,0x1a2935);
    for(unsigned i=0;i<5;i++) {
        resource_lines[i]=label(resource_panel,"",10,6+(int)i*21,653);
        lv_obj_set_style_text_font(resource_lines[i],&tv4_ru_12,0);
    }
    lv_obj_add_flag(resource_panel,LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *left=diag_left=panel(diag_panel,14,68,330,115,0x1a2935);
    muted(label(left,tr(0,S_STATE_MODEL),12,9,304));
    diag_state=label(left,"",12,30,304); lv_obj_set_style_text_font(diag_state,&tv4_ru_20,0);
    muted(label(left,tr(0,S_CAUSE),12,62,60)); diag_fault=label(left,"",82,62,236);
    diag_channel=label(left,"",12,88,306);
    lv_obj_t *right=diag_right=panel(diag_panel,356,68,335,115,0x1a2935);
    muted(label(right,tr(0,S_METRICS),12,9,310));
    diag_frames=label(right,"",12,34,310);diag_heap=label(right,"",12,58,310);diag_latency=label(right,"",12,82,310);
    muted(label(diag_panel,tr(0,S_TESTS),14,196,674));
    string_id captions[]={S_TEST_BREAK,S_TEST_CONNECT,S_TEST_EMERGENCY,S_TEST_TOUCH,S_TEST_DISPLAY,S_ACK,S_NEW_SIM};
    event_t events[]={EV_DISCONNECT,EV_CONNECT,EV_EMERGENCY,EV_TOUCH_FAULT,EV_DISPLAY_FAULT,EV_ACK,EV_NEW_SIM};
    for(unsigned i=0;i<7;i++) button(diag_panel,captions[i],events[i],14+(i%4)*172,222+(i/4)*52,160,40,false);
    muted(label(diag_panel,tr(0,S_NO_COMMANDS),530,286,160));
    muted(label(diag_panel,tr(0,S_ACK_HINT),14,328,674));
    log_panel=panel(root,89,56,705,347,0x14212b);
    title=label(log_panel,tr(0,S_LOG),14,10,674); lv_obj_set_style_text_font(title,&tv4_ru_20,0);
    muted(label(log_panel,tr(0,S_JOURNAL_SUB),14,40,674));
    log_summary=label(log_panel,"",14,63,674);
    muted(label(log_panel,tr(0,S_TIME),14,88,92));
    muted(label(log_panel,tr(0,S_EVENT),120,88,220));
    muted(label(log_panel,tr(0,S_REASON),364,88,322));
    for(int i=0;i<8;i++) {
        int y=110+i*22;
        if(!(i%2)) {
            lv_obj_t *row=panel(log_panel,10,y-1,685,21,0x1c2c38);
            lv_obj_set_style_shadow_width(row,0,0); lv_obj_set_style_border_width(row,0,0);
            lv_obj_set_style_radius(row,2,0);lv_obj_set_style_bg_grad_color(row,lv_color_hex(0x1c2c38),0);
        }
        log_time[i]=label(log_panel,"",14,y,96);
        log_event[i]=label(log_panel,"",120,y,232);
        log_reason[i]=label(log_panel,"",364,y,322);
    }
    lv_obj_t *prev=button(log_panel,S_PREVIOUS,-1,14,298,112,36,false);
    lv_obj_add_flag(prev,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(prev,page_event,LV_EVENT_CLICKED,(void *)0);
    lv_obj_t *next=button(log_panel,S_NEXT,-1,577,298,112,36,false);
    lv_obj_add_flag(next,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(next,page_event,LV_EVENT_CLICKED,(void *)1);
    log_page_label=label(log_panel,"",170,309,364);lv_obj_set_style_text_align(log_page_label,LV_TEXT_ALIGN_CENTER,0);
    wifi_panel=wifi_view_create(root);lv_obj_add_flag(wifi_panel,LV_OBJ_FLAG_HIDDEN);
    tools_panel=tools_view_create(root);lv_obj_add_flag(tools_panel,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(diag_panel,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(log_panel,LV_OBJ_FLAG_HIDDEN);
}
static void refresh_views(void) {
    const diag_record_t *r=&diagnostics.record;
    if(r->resources_valid) {
        lv_label_set_text_fmt(resource_lines[0],tr(NULL,S_DIAG_MEM),(unsigned long)(r->internal_free/1024),(unsigned long)(r->internal_min/1024),(unsigned long)(r->internal_largest/1024));
        lv_label_set_text_fmt(resource_lines[1],tr(NULL,S_DIAG_PSRAM),(unsigned long)(r->psram_free/1024),(unsigned long)(r->psram_min/1024),(unsigned long)(r->psram_total/1024));
        lv_label_set_text_fmt(resource_lines[2],tr(NULL,S_DIAG_STACK),(unsigned long)r->owner_stack,(unsigned long)r->touch_stack,(unsigned long)r->wifi_stack);
    } else {
        lv_label_set_text(resource_lines[0],tr(NULL,S_UNMEASURED));
        lv_label_set_text(resource_lines[1],"");lv_label_set_text(resource_lines[2],"");
    }
    lv_label_set_text_fmt(resource_lines[3],tr(NULL,S_DIAG_DROPS),(unsigned long)r->queue_drops,(unsigned long)r->wifi_queue_drops,(unsigned long)r->export_drops);
    lv_label_set_text_fmt(resource_lines[4],tr(NULL,S_DIAG_RESET),(unsigned long)r->reset_reason,(unsigned long)r->watchdog_boot);
    lv_label_set_text(diag_state,state_text(model.mode)); lv_label_set_text(diag_fault,fault_text(model.fault));
    lv_label_set_text_fmt(diag_channel,tr(0,S_FMT_CHANNEL),tr(0,model.connected?S_LINK_UP:S_LINK_DOWN),(unsigned long)model.rejected);
    if(metrics_valid) {
        lv_label_set_text_fmt(diag_frames,tr(0,S_FMT_FRAMES),(unsigned long)measured_fps,(unsigned long)render_ms);
        lv_label_set_text_fmt(diag_heap,tr(0,S_FMT_HEAP),(unsigned long)free_bytes);
        lv_label_set_text_fmt(diag_latency,tr(0,S_FMT_LATENCY),(unsigned long)touch_latency_ms);
    } else {
        lv_label_set_text(diag_frames,tr(0,S_METRICS_WAIT));
        lv_label_set_text(diag_heap,"");
        lv_label_set_text(diag_latency,tr(0,S_UNMEASURED));
    }
    lv_label_set_text_fmt(log_summary,tr(0,S_FMT_JOURNAL_SUMMARY),model.journal.count,(unsigned long)model.journal.overwritten);
    unsigned pages=(model.journal.count+7)/8;if(!pages)pages=1;if(log_page>=pages)log_page=pages-1;
    lv_label_set_text_fmt(log_page_label,tr(0,S_FMT_PAGE),log_page+1,pages);
    static const string_id kinds[]={S_J_BOOT,S_J_STEP,S_J_STOP,S_J_DISCONNECT,S_J_CONNECT,S_J_FAULT,S_J_ACK,S_J_INCREMENT,S_J_REJECTED,S_J_TIMEOUT,S_J_RESTART,S_J_TOOL_SAVE,S_J_TOOL_SELECT,S_SPINDLE_SET,S_OVERRIDE,S_J_FRAME,S_J_ZERO_X,S_J_ZERO_Z,S_J_ZERO_ALL};
    for(unsigned i=0;i<8;i++) {
        journal_entry_t entry;
        if(journal_get(&model.journal,log_page*8+i,&entry)) {
            lv_label_set_text_fmt(log_time[i],tr(0,S_FMT_TIME),(unsigned long)entry.ms);
            lv_label_set_text(log_event[i],tr(0,(unsigned)entry.kind<sizeof(kinds)/sizeof(*kinds)?kinds[entry.kind]:S_J_REJECTED));
            lv_label_set_text(log_reason[i],entry.fault==F_NONE?"":fault_text(entry.fault));
        } else {
            lv_label_set_text(log_time[i],"");lv_label_set_text(log_event[i],"");lv_label_set_text(log_reason[i],"");
        }
    }
}
void dashboard_create(void) {
    settings_panel=NULL;firmware_view=(firmware_view_t){0};firmware_info_reset(&firmware_info);
    zero_requested=zero_close=false;zero_dialog=NULL;
    override_pending=false;
    resource_view=resource_requested=false;
    diagnostics_init(&diagnostics,lv_tick_get());
    model_init(&model); read_pos=write_pos=used=0; pending_fault=EV_STEP; dropped_events=0; view_valid=false; view=VIEW_HOME; view_requested=false; home_count=log_page=0; metrics_valid=false; metrics_revision=rendered_metrics=0;
    /* Reference-matching deterministic demo readings, never live machine data. */
    model.x_um=25430; model.z_um=-142850; model.step_um=10; model.rpm=1250; model.feed=180;
    lv_obj_t *s=lv_screen_active(); lv_obj_remove_style_all(s);
    lv_obj_set_style_bg_opa(s,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(s,lv_color_hex(0x03080c),0);
    lv_obj_set_style_text_color(s,lv_color_hex(0xffffff),0);
    lv_obj_set_style_text_font(s,&tv4_ru_12,0);
    panel(s,0,0,800,46,0x24323f);
    lv_obj_t *logo=label(s,"TV4",22,1,57);
    lv_obj_set_style_text_font(logo,&tv4_bold_24,0);
    lv_obj_t *cnc=label(s,"CNC",80,4,57);
    lv_obj_set_style_text_font(cnc,&tv4_ru_20,0);
    lv_obj_set_style_text_color(cnc,lv_color_hex(0x009fff),0);
    muted(label(s,tr(0,S_LATHE),25,31,116));
    status=label(s,"",134,16,72);
    lv_obj_set_style_text_color(status,lv_color_hex(0xffcb61),0);
    line(s,207,8,1,29);
    lv_obj_t *tab=button(s,S_MANUAL_JOG,-1,220,6,124,34,true);
    icon(tab,&tv4_icon_hand,9,6,22);
    lv_obj_align(lv_obj_get_child(tab,0),LV_ALIGN_RIGHT_MID,-6,0);
    tab=button(s,S_PROGRAM,-1,350,6,124,34,false);
    icon(tab,&tv4_icon_code,9,7,20);
    lv_obj_align(lv_obj_get_child(tab,0),LV_ALIGN_RIGHT_MID,-9,0);
    frame_control=button(s,S_MACHINE_TAG,EV_FRAME_TOGGLE,480,6,98,34,false);
    line(s,590,8,1,29);
    lv_obj_t *time=label(s,"18:24",603,5,66);
    lv_obj_set_style_text_font(time,&tv4_ru_14,0);
    muted(label(s,tr(0,S_DATE),603,26,78));
    icon(s,&tv4_icon_wifi,685,15,20);
    lv_obj_t *wifi_icon=lv_obj_get_child(s,-1);wifi_view_set_indicator(wifi_icon);
    lv_obj_add_flag(wifi_icon,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_ext_click_area(wifi_icon,10);
    lv_obj_add_event_cb(wifi_icon,nav_event,LV_EVENT_CLICKED,(void *)(uintptr_t)VIEW_WIFI);
    lv_obj_t *can=panel(s,711,8,37,30,0x0b231c);
    muted(label(can,"CAN",5,8,32));
    lv_obj_t *gear=panel(s,760,7,32,32,0x23303c); icon(gear,&tv4_icon_gear,5,5,22);
    lv_obj_add_flag(gear,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gear,nav_event,LV_EVENT_CLICKED,(void *)(uintptr_t)VIEW_SETTINGS);
    panel(s,0,48,79,355,0x121e28);
    string_id nav[]={S_HOME,S_PROGRAM,S_TOOLS,S_SETTINGS,S_DIAG,S_LOG};
    const lv_image_dsc_t *icons[]={&tv4_icon_home,&tv4_icon_code,&tv4_icon_tool,&tv4_icon_sliders,&tv4_icon_pulse,&tv4_icon_file};
    for(unsigned i=0;i<6;i++) {
        int y=60+(int)i*56;
        lv_obj_t *n=button(s,nav[i],-1,2,y,75,51,i==0); navigation[i]=n;
        lv_obj_set_style_text_font(n,&tv4_ru_12,0);
        lv_obj_set_style_bg_color(n,lv_color_hex(0x003b66),0);
        lv_obj_set_style_border_color(n,lv_color_hex(0x00c8ff),0);
        lv_obj_set_style_shadow_color(n,lv_color_hex(0x009fff),0);
        lv_obj_t *t=lv_obj_get_child(n,0);lv_obj_align(t,LV_ALIGN_BOTTOM_MID,0,-4);
        icon(n,icons[i],27,6,21);
        if(i==0 || i==2 || i==3 || i==4 || i==5) {
            lv_obj_add_flag(n,LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(n,nav_event,LV_EVENT_CLICKED,(void *)(uintptr_t)(i==0?VIEW_HOME:i==2?VIEW_TOOLS:i==3?VIEW_SETTINGS:i==4?VIEW_DIAGNOSTICS:VIEW_LOG));
        }
        if(i) {
            lv_obj_set_style_bg_opa(n,LV_OPA_TRANSP,0);lv_obj_set_style_border_opa(n,LV_OPA_TRANSP,0);
            lv_obj_set_style_shadow_opa(n,LV_OPA_TRANSP,0);muted(t);
        }
    }
    unsigned home_start=lv_obj_get_child_count(s);
    for(int i=0;i<2;i++) {
        lv_obj_t *p=panel(s,89,56+i*66,343,59,0x14212b);
        lv_obj_t *accent=panel(p,0,0,3,59,0x00b8fa); lv_obj_set_style_radius(accent,0,0);
        lv_obj_t *axis=label(p,tr(0,i?S_Z:S_X),20,4,44);
        lv_obj_set_style_text_font(axis,&tv4_digits_36,0);
        lv_obj_set_style_text_color(axis,lv_color_hex(0x62bdff),0);
        muted(label(p,"мм",23,43,30));
        lv_obj_t *v=label(p,"",73,6,202);
        lv_obj_set_style_text_font(v,&tv4_digits_36,0);
        lv_obj_set_style_text_align(v,LV_TEXT_ALIGN_RIGHT,0);
        if(i) z_value=v; else x_value=v;
        lv_obj_t *target=panel(p,296,11,36,36,0x1d2b36);zero_targets[i]=target;
        icon(target,&tv4_icon_target,7,7,22);lv_obj_add_flag(target,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_ext_click_area(target,6);
        lv_obj_add_event_cb(target,request_zero,LV_EVENT_CLICKED,(void *)(uintptr_t)(i?EV_ZERO_WORK_Z:EV_ZERO_WORK_X));
    }
    /* Three metric cards share an 8 px gutter and separate text/indicator lanes. */
    lv_obj_t *p=panel(s,89,188,142,66,0x17262f);
    icon(p,&tv4_icon_spindle,8,6,16); muted(label(p,tr(0,S_SPINDLE_SHORT),30,6,66));
    rpm_value=label(p,"",12,28,70); lv_obj_set_style_text_font(rpm_value,&tv4_bold_24,0);
    muted(label(p,tr(0,S_RPM_UNIT),88,43,48));
    lv_obj_t *arc=lv_arc_create(p); rpm_arc=arc; lv_obj_set_pos(arc,99,6); lv_obj_set_size(arc,35,35);
    lv_arc_set_bg_angles(arc,205,335); lv_arc_set_range(arc,0,100); lv_arc_set_value(arc,72);
    lv_obj_set_style_arc_width(arc,5,LV_PART_MAIN); lv_obj_set_style_arc_width(arc,5,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc,lv_color_hex(0x41e82f),LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc,lv_color_hex(0x3c4b59),LV_PART_MAIN);
    lv_obj_remove_style(arc,NULL,LV_PART_KNOB); lv_obj_remove_flag(arc,LV_OBJ_FLAG_CLICKABLE);
    p=panel(s,239,188,94,66,0x17262f); icon(p,&tv4_icon_wave,8,6,16);
    muted(label(p,tr(0,S_FEED_REV),30,6,56));
    lv_obj_t *v=label(p,"0.150",10,23,74); feed_rev_value=v; lv_obj_set_style_text_font(v,&tv4_ru_20,0);
    muted(label(p,tr(0,S_MM_REV),10,48,74));
    p=panel(s,341,188,91,66,0x17262f); icon(p,&tv4_icon_gauge,8,6,16);
    muted(label(p,tr(0,S_SPEED),30,6,56));
    speed_value=label(p,"",10,23,71); lv_obj_set_style_text_font(speed_value,&tv4_ru_20,0);
    muted(label(p,tr(0,S_FEED_UNIT),10,48,71));
    p=panel(s,444,56,350,198,0x14212b);
    lv_obj_t *photo=lv_image_create(p); lv_image_set_src(photo,&tv4_lathe); lv_obj_set_pos(photo,1,1);
    lv_obj_set_style_radius(p,9,0); lv_obj_set_style_clip_corner(p,true,0);
    lv_obj_t *tool=panel(p,198,3,149,192,0x15222c);
    icon(tool,&tv4_icon_tool,13,12,18); icon(tool,&tv4_icon_insert,112,13,24);
    v=label(tool,"",42,12,56);home_tool_id=v; lv_obj_set_style_text_font(v,&tv4_ru_20,0);
    home_tool_name=label(tool,"",13,39,131); home_tool_code=label(tool,"",13,54,130);muted(home_tool_code);
    line(tool,12,73,124,1);
    const char *marks[]={"Ø","↔","R"}; const char *dims[]={"32.000", "0.200", "0.400"};
    for(int i=0;i<3;i++) {
        label(tool,marks[i],16,81+i*25,22);
        v=label(tool,dims[i],45,79+i*25,63);if(i==2)home_tool_radius=v; lv_obj_set_style_text_font(v,&tv4_ru_14,0);
        muted(label(tool,"мм",111,82+i*25,30)); line(tool,12,97+i*25,124,1);
    }
    lv_obj_t *layers=panel(tool,115,157,24,24,0x172630);icon(layers,&tv4_icon_layers,4,4,16);
    panel(s,89,264,705,139,0x17252e);
    button(s,S_JOG_TAB,-1,101,272,74,28,true);
    button(s,S_STEP_TAB,-1,179,272,73,28,false);
    button(s,S_CONTINUOUS,-1,256,272,91,28,false);
    string_id inc[]={S_INC_1,S_INC_10,S_INC_100,S_INC_1000};
    for(int i=0;i<4;i++) {
        increments[i]=button(s,inc[i],EV_INC_1+i,101+i*54,308,51,28,i==1);
        lv_obj_set_ext_click_area(increments[i],10);
    }
    muted(label(s,"мм",326,315,25)); line(s,358,276,1,60);
    icon(s,&tv4_icon_gauge,373,277,18); label(s,tr(0,S_OVERRIDE),399,278,139);
    override_value=label(s,"100%",547,278,46);
    lv_obj_t *slider=lv_slider_create(s); override_slider=slider; lv_obj_set_pos(slider,377,321); lv_obj_set_size(slider,204,8);
    lv_slider_set_range(slider,0,150); lv_slider_set_value(slider,100,LV_ANIM_OFF); lv_obj_add_flag(slider,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(slider,12);
    lv_obj_add_event_cb(slider,override_changed,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_set_style_bg_color(slider,lv_color_hex(0x009dff),LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider,lv_color_hex(0x49b8ff),LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider,5,LV_PART_KNOB);
    line(s,603,276,1,60);
    icon(s,&tv4_icon_gauge,616,277,18); label(s,tr(0,S_SPINDLE_SET),642,278,136);
    v=label(s,"1250",616,307,54); rpm_set_value=v; lv_obj_set_style_text_font(v,&tv4_ru_20,0);
    muted(label(s,tr(0,S_RPM_UNIT),676,315,40));
    lv_obj_t *minus=button(s,S_X_MINUS,EV_RPM_MINUS,724,306,26,30,false);
    lv_label_set_text(lv_obj_get_child(minus,0),"−");
    lv_obj_t *plus=button(s,S_X_PLUS,EV_RPM_PLUS,758,306,26,30,false);
    lv_label_set_text(lv_obj_get_child(plus,0),"+");
    line(s,101,343,681,1);
    string_id jog[]={S_X_MINUS,S_X_PLUS,S_Z_MINUS,S_Z_PLUS};
    event_t events[]={EV_X_MINUS,EV_STEP,EV_Z_MINUS,EV_Z_PLUS};
    int xpos[]={100,181,287,370};
    for(int i=0;i<4;i++) {
        lv_obj_t *b=button(s,jog[i],events[i],xpos[i],351,75,44,false);
        lv_obj_set_style_text_font(b,&tv4_bold_24,0);
        lv_obj_set_style_text_color(b,lv_color_hex(0xb4e2ff),0);
        lv_obj_set_style_border_color(b,lv_color_hex(0x009cff),0);
        lv_obj_set_style_border_width(b,2,0);
    }
    line(s,270,352,1,42);line(s,459,352,1,42);
    icon_button(s,S_ZERO,&tv4_icon_target,472,347,95,48,-1);
    zero_all_button=icon_button(s,S_WORK_ZERO,&tv4_icon_target,575,347,103,48,-1);
    lv_obj_add_flag(zero_all_button,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(zero_all_button,request_zero,LV_EVENT_CLICKED,(void *)(uintptr_t)EV_ZERO_WORK_ALL);
    icon_button(s,S_RESET,&tv4_icon_reset,687,347,99,48,EV_ACK);
    for(unsigned i=home_start;i<lv_obj_get_child_count(s);i++) {
        if(home_count<96) home_objects[home_count++]=lv_obj_get_child(s,i);
    }
    create_views(s);
    panel(s,5,415,594,44,0x172630);
    /* These footer readouts are explicitly sample values under SIMULATION. */
    icon(s,&tv4_icon_alert,14,429,19);
    label(s,tr(0,S_CONTROLLER),39,433,85);
    muted(label(s,tr(0,S_OFF),126,433,72)); line(s,208,428,1,23);
    icon(s,&tv4_icon_thermo,224,429,18); muted(label(s,tr(0,S_TEMPERATURE),250,433,82));
    label(s,"36 °C",340,433,52);line(s,400,427,1,23);
    icon(s,&tv4_icon_bolt,416,429,17); muted(label(s,tr(0,S_VOLTAGE),441,433,80));
    label(s,"24.1 В",533,433,60);
    /* Hazard stripes are decorative; a screen STOP is never a safety E-stop. */
    lv_obj_t *hazard=panel(s,607,413,187,49,0xdec028);
    lv_obj_set_style_bg_grad_color(hazard,lv_color_hex(0xb6900a),0);
    for(int i=0;i<2;i++) for(int j=0;j<4;j++) line(s,609+i*179,414+j*12,5,7);
    lv_obj_t *stop=button(s,S_STOP,EV_STOP,619,413,163,49,false);
    icon(stop,&tv4_icon_alert,10,5,23);
    v=lv_obj_get_child(stop,0); lv_obj_set_style_text_font(v,&tv4_ru_20,0); lv_obj_align(v,LV_ALIGN_TOP_MID,14,3);
    v=label(stop,tr(0,S_STOP_NOTE),6,31,153);lv_obj_set_style_text_align(v,LV_TEXT_ALIGN_CENTER,0);
    coordinate_frame=label(s,"",91,465,340);muted(coordinate_frame);
    lv_obj_add_flag(coordinate_frame,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_ext_click_area(coordinate_frame,5);
    lv_obj_add_event_cb(coordinate_frame,click,LV_EVENT_CLICKED,(void *)(uintptr_t)EV_FRAME_TOGGLE);
    v=label(s,tr(0,S_SIM),470,465,126); lv_obj_set_style_text_color(v,lv_color_hex(0xffcb61),0);
    frames=0; log_ms=lv_tick_get();
    lv_display_add_event_cb(lv_display_get_default(),render_event,LV_EVENT_ALL,NULL);
    dashboard_update();
}
void dashboard_update(void) {
    if(resource_requested) {
        resource_view=!resource_view;resource_requested=false;view_valid=false;
        if(resource_view) {
            lv_obj_remove_flag(resource_panel,LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(diag_left,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(diag_right,LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(resource_panel,LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(diag_left,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(diag_right,LV_OBJ_FLAG_HIDDEN);
        }
    }
    apply_view();
    bool was_connected=model.connected;
    model_tick(&model,lv_tick_get());
    if(was_connected && !model.connected)diag_increment(&diagnostics.record.mock_can_timeouts);
    while(used) {
        touch_latency_ms=lv_tick_elaps(event_ms[read_pos]);
        diagnostics_event(&diagnostics,touch_latency_ms);
        model_event(&model,queue[read_pos]); read_pos=(read_pos+1)%8; used--;
    }
    model.rejected+=dropped_events; dropped_events=0;
    if(pending_fault!=EV_STEP) { model_event(&model,pending_fault); pending_fault=EV_STEP; }
    if(override_pending) { model_set_feed_override(&model,pending_override); override_pending=false; view_valid=false; }
    update_zero_dialog();
    tools_view_update(&model);
    wifi_view_update();
    if(diagnostics_window(&diagnostics,lv_tick_get()))metrics_revision++;
    uint32_t elapsed=lv_tick_elaps(log_ms);
    if(elapsed>=1000) {
        free_bytes=0;
#ifdef ESP_PLATFORM
        free_bytes=esp_get_free_heap_size();
#else
        lv_mem_monitor_t mem; lv_mem_monitor(&mem); free_bytes=mem.free_size;
#endif
        measured_fps=frames*1000/elapsed; metrics_valid=true; metrics_revision++;
        frames=0; log_ms=lv_tick_get();
    }
    /* Do not allocate/repaint unchanged text on every owner-loop iteration. */
    if(view_valid && rendered.mode==model.mode && rendered.x_um==model.x_um &&
       rendered.z_um==model.z_um && rendered.work_x_um==model.work_x_um &&
       rendered.work_z_um==model.work_z_um && rendered.work_frame==model.work_frame && rendered.rpm==model.rpm &&
       rendered.feed==model.feed && rendered.feed_override==model.feed_override && rendered.step_um==model.step_um && rendered.fault==model.fault &&
       rendered.tools.revision==model.tools.revision && rendered.tool==model.tool &&
       rendered.connected==model.connected && rendered.rejected==model.rejected &&
       rendered.journal.total==model.journal.total && (view!=VIEW_DIAGNOSTICS || rendered_metrics==metrics_revision)) return;
    rendered=model; view_valid=true; rendered_metrics=metrics_revision;
    refresh_views();
    static const string_id tool_names[]={S_TOOL_NAME,S_TOOL_NAME_2,S_TOOL_NAME_3,S_TOOL_NAME_4};
    static const string_id tool_codes[]={S_TOOL_CODE_1,S_TOOL_CODE_2,S_TOOL_CODE_3,S_TOOL_CODE_4};
    unsigned active_tool=model.tools.active;
    lv_label_set_text_fmt(home_tool_id,tr(0,S_FMT_TOOL_ID),active_tool+1);
    lv_label_set_text(home_tool_name,tr(0,tool_names[active_tool]));
    lv_label_set_text(home_tool_code,tr(0,tool_codes[active_tool]));
    long radius=model.tools.geometry[active_tool].radius_um;
    lv_label_set_text_fmt(home_tool_radius,"%ld.%03ld",radius/1000,radius%1000);
    string_id id=S_DEMO;
    switch(model.mode) {
    case DISCONNECTED:id=S_DISCONNECTED;break;
    case READY:id=S_READY;break;
    case HOLD:id=S_PAUSE;break;
    case FAULT:id=S_FAULT;break;
    default:break;
    }
    lv_label_set_text(status,tr(0,id));
    lv_label_set_text(coordinate_frame,tr(NULL,model.work_frame?S_WORK_FRAME:S_MACHINE_FRAME));
    lv_label_set_text(lv_obj_get_child(frame_control,0),tr(NULL,model.work_frame?S_WORK_TAG:S_MACHINE_TAG));
    lv_obj_set_style_text_color(coordinate_frame,lv_color_hex(model.work_frame?0x46c5ff:0xc1d6e8),0);
    bool zero_allowed=model.mode==SIMULATION && !model.connected;
    for(unsigned i=0;i<3;i++) {
        lv_obj_t *o=i==2?zero_all_button:zero_targets[i];
        if(zero_allowed)lv_obj_remove_state(o,LV_STATE_DISABLED);else lv_obj_add_state(o,LV_STATE_DISABLED);
        lv_obj_set_style_opa(o,zero_allowed?LV_OPA_COVER:LV_OPA_40,0);
    }
    long x=model_display_x(&model), z=model_display_z(&model);
    bool wide=x<=-1000000 || x>=1000000 || z<=-1000000 || z>=1000000;
    lv_obj_set_style_text_font(x_value,wide?&tv4_digits_32:&tv4_digits_36,0);
    lv_obj_set_style_text_font(z_value,wide?&tv4_digits_32:&tv4_digits_36,0);
    lv_obj_set_y(x_value,wide?10:6);lv_obj_set_y(z_value,wide?10:6);
    lv_label_set_text_fmt(x_value,"%s%03ld.%03ld", x<0?"-":"+", (x<0?-x:x)/1000, (x<0?-x:x)%1000);
    lv_label_set_text_fmt(z_value,"%s%03ld.%03ld", z<0?"-":"+", (z<0?-z:z)/1000, (z<0?-z:z)%1000);
    lv_arc_set_value(rpm_arc,model.rpm?72:0);
    lv_label_set_text_fmt(rpm_value,"%u",model.rpm);
    lv_label_set_text_fmt(speed_value,"%u",model.feed);
    lv_label_set_text_fmt(rpm_set_value,"%u",model.rpm);
    unsigned feed_per_rev=model.rpm?(unsigned)model.feed*1000u/model.rpm:0;
    lv_label_set_text_fmt(feed_rev_value,"%u.%03u",feed_per_rev/1000,feed_per_rev%1000);
    lv_label_set_text_fmt(override_value,"%u%%",model.feed_override);
    lv_slider_set_value(override_slider,model.feed_override,LV_ANIM_OFF);
    if(model.mode==SIMULATION && !model.connected) lv_obj_remove_state(override_slider,LV_STATE_DISABLED);
    else lv_obj_add_state(override_slider,LV_STATE_DISABLED);
    int32_t steps[]={1,10,100,1000};
    for(int i=0;i<4;i++) {
        lv_obj_set_style_bg_color(increments[i],lv_color_hex(model.step_um==steps[i]?0x009fff:0x354451),0);
        lv_obj_set_style_bg_grad_color(increments[i],lv_color_hex(model.step_um==steps[i]?0x0052bb:0x111c25),0);
        lv_obj_set_style_shadow_color(increments[i],lv_color_hex(model.step_um==steps[i]?0x009cff:0x000000),0);
        lv_obj_set_style_shadow_width(increments[i],model.step_um==steps[i]?14:10,0);
        lv_obj_set_style_shadow_opa(increments[i],model.step_um==steps[i]?LV_OPA_50:LV_OPA_90,0);
        lv_obj_set_style_border_color(increments[i],lv_color_hex(model.step_um==steps[i]?0x40d9ff:0x657d91),0);
    }
}

model_t dashboard_snapshot(void) { return model; }
