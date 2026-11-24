#include "firmware_view.h"
#include "strings.h"
#include <string.h>
LV_FONT_DECLARE(tv4_ru_12);
LV_FONT_DECLARE(tv4_ru_14);
LV_FONT_DECLARE(tv4_ru_20);

/* LVGL9 invalidates and reallocates even when the text is unchanged. */
static void label_text(lv_obj_t *label,const char *value) {
    if(strcmp(lv_label_get_text(label),value))lv_label_set_text(label,value);
}
static uint32_t received_kib(const tv4_update_snapshot *s) {
    return s->received>=s->total?(s->total+1023)/1024:s->received/1024;
}
static lv_obj_t *text(lv_obj_t *parent,const char *value,int x,int y,int width,const lv_font_t *font,uint32_t color) {
    lv_obj_t *o=lv_label_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,width);
    lv_label_set_text(o,value);lv_obj_set_style_text_font(o,font,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);return o;
}
static void separator(lv_obj_t *parent,int y) {
    lv_obj_t *line=lv_obj_create(parent);lv_obj_remove_style_all(line);
    lv_obj_set_pos(line,18,y);lv_obj_set_size(line,669,1);
    lv_obj_set_style_bg_opa(line,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(line,lv_color_hex(0x344752),0);
    lv_obj_remove_flag(line,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
}
void firmware_view_create(firmware_view_t *view,lv_obj_t *parent,
    lv_event_cb_t back,void *back_context) {
    if(view->panel)return;
    view->panel=lv_obj_create(parent);lv_obj_remove_style_all(view->panel);
    lv_obj_set_pos(view->panel,89,56);lv_obj_set_size(view->panel,705,347);
    lv_obj_set_style_bg_opa(view->panel,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(view->panel,lv_color_hex(0x14212b),0);
    lv_obj_set_style_bg_grad_color(view->panel,lv_color_hex(0x080f17),0);
    lv_obj_set_style_bg_grad_dir(view->panel,LV_GRAD_DIR_VER,0);
    lv_obj_set_style_border_width(view->panel,1,0);
    lv_obj_set_style_border_color(view->panel,lv_color_hex(0x344752),0);
    lv_obj_set_style_radius(view->panel,8,0);
    lv_obj_remove_flag(view->panel,LV_OBJ_FLAG_SCROLLABLE);
    text(view->panel,tr(NULL,S_FW_TITLE),18,13,500,&tv4_ru_20,0xffffff);
    view->source=text(view->panel,tr(NULL,S_FW_UNKNOWN),18,49,520,&tv4_ru_12,0xffcb61);
    lv_obj_t *button=lv_button_create(view->panel);lv_obj_remove_style_all(button);
    lv_obj_set_pos(button,552,10);lv_obj_set_size(button,135,44);
    lv_obj_set_style_radius(button,7,0);lv_obj_set_style_bg_opa(button,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(button,lv_color_hex(0x354451),0);
    lv_obj_set_style_bg_grad_color(button,lv_color_hex(0x111c25),0);
    lv_obj_set_style_bg_grad_dir(button,LV_GRAD_DIR_VER,0);
    lv_obj_set_style_border_width(button,1,0);
    lv_obj_set_style_border_color(button,lv_color_hex(0x657d91),0);
    lv_obj_set_style_bg_color(button,lv_color_hex(0x1486b0),LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(button,8,0);
    lv_obj_set_style_shadow_color(button,lv_color_hex(0x000000),0);
    lv_obj_t *caption=text(button,tr(NULL,S_FW_BACK),0,0,120,&tv4_ru_12,0xffffff);
    lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(caption);
    lv_obj_add_event_cb(button,back,LV_EVENT_CLICKED,back_context);
    separator(view->panel,76);
    static const string_id names[]={S_FW_BUILD,S_FW_SDK,S_FW_PROFILE,S_FW_STM32,S_FW_CAN};
    for(unsigned i=0;i<5;i++) {
        text(view->panel,tr(NULL,names[i]),20,89+(int)i*31,150,&tv4_ru_14,0x9fb4c8);
        view->value[i]=text(view->panel,tr(NULL,S_FW_UNKNOWN),180,89+(int)i*31,505,&tv4_ru_14,0xc1d6e8);
    }
    separator(view->panel,248);
    view->update_state=text(view->panel,tr(NULL,S_FW_UPDATE_OFF),20,266,665,&tv4_ru_14,0xffcb61);
    view->update_note=text(view->panel,tr(NULL,S_FW_UPDATE_NOTE),20,292,665,&tv4_ru_12,0x9fb4c8);
}
void firmware_view_update(firmware_view_t *view,const firmware_info_t *info) {
    if(!view->panel)return;
    static const string_id profiles[]={S_FW_UNKNOWN,S_FW_HEADLESS,S_FW_REV12,S_FW_MEMORY_BENCH,S_FW_TLS_BENCH,S_OTA_PROFILE};
    string_id source=info->source==FIRMWARE_INFO_DEVICE?S_FW_DEVICE:
        info->source==FIRMWARE_INFO_SIMULATOR?S_FW_SIMULATOR:S_FW_UNKNOWN;
    bool known=info->source!=FIRMWARE_INFO_UNKNOWN;
    label_text(view->source,tr(NULL,source));
    lv_obj_set_style_text_color(view->source,lv_color_hex(info->source==FIRMWARE_INFO_DEVICE?0x9fb4c8:0xffcb61),0);
    label_text(view->value[0],known?info->build:tr(NULL,S_FW_UNKNOWN));
    label_text(view->value[1],known?info->sdk:tr(NULL,S_FW_UNKNOWN));
    unsigned profile=(unsigned)info->profile;
    label_text(view->value[2],tr(NULL,profile<=FIRMWARE_PROFILE_OTA?profiles[profile]:S_FW_UNKNOWN));
    label_text(view->value[3],tr(NULL,S_FW_PEER_UNKNOWN));
    label_text(view->value[4],tr(NULL,S_FW_PEER_UNKNOWN));
}
void firmware_view_destroy(firmware_view_t *view) {
    if(view->panel)lv_obj_delete(view->panel);
    memset(view,0,sizeof(*view));
}

static void ota_action(lv_event_t *event) {
    firmware_view_t *v=lv_event_get_user_data(event);tv4_update_snapshot s;
    tv4_ota_service_snapshot(&s);lv_obj_t *button=lv_event_get_target(event);
    if(button==v->action[0]) {v->confirm=false;tv4_ota_service_request(TV4_UPDATE_CHECK);}
    else if(button==v->action[2]) {
        if(v->confirm)v->confirm=false;else tv4_ota_service_request(TV4_UPDATE_CANCEL);
    }else if(s.state==TV4_UPDATE_RESTART)tv4_ota_service_request(TV4_UPDATE_REBOOT);
    else if(s.state==TV4_UPDATE_READY && s.install_allowed) {
        if(v->confirm && !memcmp(v->confirm_digest,s.sha256,32)) {
            v->confirm=false;tv4_ota_service_request(TV4_UPDATE_INSTALL);
        }else {v->confirm=true;memcpy(v->confirm_digest,s.sha256,32);}
    }
    v->refresh=true;
}
void firmware_view_ota_update(firmware_view_t *v,const tv4_update_snapshot *s) {
    if(!v->panel || !s || !s->enabled)return;
    /* Revoke confirmation before any progress coalescing. Only presentation is
     * rate limited; commands/admission, identity, errors and state transitions
     * keep their existing immediate handling. */
    if(v->confirm && (s->state!=TV4_UPDATE_READY || !s->install_allowed || memcmp(v->confirm_digest,s->sha256,32))) {
        v->confirm=false;v->refresh=true;
    }
    const tv4_update_snapshot *r=&v->rendered;
    bool changed=v->refresh || !v->action[0] || r->state!=s->state || r->error!=s->error ||
        r->install_allowed!=s->install_allowed || r->total!=s->total ||
        memcmp(r->version,s->version,sizeof(s->version)) || memcmp(r->sha256,s->sha256,32);
    uint32_t now=lv_tick_get();
    if(!changed) {
        if(s->state!=TV4_UPDATE_WRITING || received_kib(r)==received_kib(s))return;
        /* At most 5 progress redraws/s while streaming. Completion is immediate.
         * Unsigned elapsed preserves the bound across the LVGL tick wrap. */
        if(s->received<s->total && (uint32_t)(now-v->progress_ms)<200)return;
    }
    v->rendered=*s;v->progress_ms=now;v->refresh=false;
    if(!v->action[0]) {
        lv_obj_add_flag(v->update_note,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(v->update_state,20,259);
        lv_obj_set_height(v->update_state,24);
        lv_label_set_long_mode(v->update_state,LV_LABEL_LONG_CLIP);
        for(unsigned i=0;i<3;i++) {
            lv_obj_t *b=lv_button_create(v->panel);v->action[i]=b;
            lv_obj_set_pos(b,18+(int)i*228,295);lv_obj_set_size(b,212,44);
            lv_obj_set_style_bg_color(b,lv_color_hex(0x096cb1),0);
            lv_obj_set_style_opa(b,LV_OPA_50,LV_STATE_DISABLED);
            v->caption[i]=text(b,"",0,0,198,&tv4_ru_12,0xffffff);
            lv_obj_set_style_text_align(v->caption[i],LV_TEXT_ALIGN_CENTER,0);lv_obj_center(v->caption[i]);
            lv_obj_add_event_cb(b,ota_action,LV_EVENT_CLICKED,v);
        }
    }
    static const string_id states[]={S_FW_UPDATE_OFF,S_OTA_SETUP,S_OTA_BOOT,S_OTA_IDLE,S_OTA_CHECKING,
        S_OTA_READY,S_OTA_WRITING,S_OTA_COMMIT,S_OTA_RESTART,S_OTA_ERROR,S_OTA_BOOT_FAILED};
    if(v->confirm)lv_label_set_text(v->update_state,tr(NULL,S_OTA_CONFIRM_NOTE));
    else if(s->state==TV4_UPDATE_READY)lv_label_set_text_fmt(v->update_state,tr(NULL,S_OTA_READY),s->version,(unsigned long)((s->total+1023)/1024));
    else if(s->state==TV4_UPDATE_WRITING)lv_label_set_text_fmt(v->update_state,tr(NULL,S_OTA_WRITING),(unsigned long)received_kib(s),(unsigned long)((s->total+1023)/1024));
    else if(s->state==TV4_UPDATE_ERROR && s->error==TV4_UPDATE_CANCELLED_ERROR)lv_label_set_text(v->update_state,tr(NULL,S_OTA_CANCELLED));
    else lv_label_set_text(v->update_state,tr(NULL,s->state<=TV4_UPDATE_BOOT_FAILED?states[s->state]:S_OTA_ERROR));
    bool allow[]={s->state==TV4_UPDATE_IDLE || s->state==TV4_UPDATE_READY || s->state==TV4_UPDATE_ERROR,
        (s->state==TV4_UPDATE_READY && s->install_allowed) || s->state==TV4_UPDATE_RESTART,
        v->confirm || s->state==TV4_UPDATE_CHECKING || s->state==TV4_UPDATE_WRITING};
    const string_id captions[]={S_OTA_CHECK,s->state==TV4_UPDATE_RESTART?S_OTA_REBOOT:!s->install_allowed?S_OTA_CHECK_ONLY:v->confirm?S_OTA_CONFIRM:S_OTA_INSTALL,S_CANCEL};
    for(unsigned i=0;i<3;i++) {
        label_text(v->caption[i],tr(NULL,captions[i]));
        bool disabled=lv_obj_has_state(v->action[i],LV_STATE_DISABLED);
        if(disabled && allow[i])lv_obj_remove_state(v->action[i],LV_STATE_DISABLED);
        else if(!disabled && !allow[i])lv_obj_add_state(v->action[i],LV_STATE_DISABLED);
    }
}
