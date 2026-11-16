#include "wifi_view.h"
#include "wifi_service.h"
#include "strings.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(tv4_ru_12);
#define WIFI_ROWS (WIFI_UI_NETWORKS+WIFI_SAVED_MAX)
static lv_obj_t *panel,*status,*ssid,*password,*keyboard,*list,*networks[WIFI_ROWS];
static wifi_snapshot_t displayed;
static lv_obj_t *indicator;
void wifi_view_set_indicator(lv_obj_t *icon) { indicator=icon; }
static char row_names[WIFI_ROWS][33];
static unsigned row_count,revision=~0u;
static uint32_t last_scan;
static bool saved_name(const char *name) {
    for(unsigned i=0;i<displayed.saved_count;i++)if(!strcmp(name,displayed.saved_ssids[i]))return true;
    return false;
}
static lv_obj_t *text(lv_obj_t *p,const char *s,int x,int y,int w) {
    lv_obj_t *o=lv_label_create(p);lv_label_set_text(o,s);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);return o;
}
static void focus(lv_event_t *e) { lv_keyboard_set_textarea(keyboard,lv_event_get_target(e));lv_obj_remove_flag(keyboard,LV_OBJ_FLAG_HIDDEN); }
static void key_event(lv_event_t *e) {
    if(lv_event_get_code(e)==LV_EVENT_READY || lv_event_get_code(e)==LV_EVENT_CANCEL) {
        lv_keyboard_set_textarea(keyboard,NULL);lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);
    }
}
static void action(lv_event_t *e) {
    unsigned kind=(unsigned)(uintptr_t)lv_event_get_user_data(e);bool ok=false;
    const char *name=lv_textarea_get_text(ssid),*secret=lv_textarea_get_text(password);
    if(kind==1) { ok=wifi_service_scan();if(ok)last_scan=lv_tick_get(); }
    if(kind==2) {
        ok=!*secret && saved_name(name)?wifi_service_connect_saved(name):wifi_service_connect(name,secret);
        lv_textarea_set_text(password,"");
    }
    if(kind==3)ok=wifi_service_disconnect();
    if(kind==4) { ok=wifi_service_forget(name);lv_textarea_set_text(password,""); }
    lv_label_set_text(status,tr(NULL,ok?S_WIFI_PENDING:S_WIFI_REJECTED));
}
static lv_obj_t *button(lv_obj_t *p,const char *s,int x,int y,int w,lv_event_cb_t cb,uintptr_t data) {
    lv_obj_t *b=lv_button_create(p);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,36);
    lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,s);lv_obj_center(l);
    lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,(void *)data);return b;
}
static void select_network(lv_event_t *e) {
    unsigned i=(unsigned)(uintptr_t)lv_event_get_user_data(e);if(i>=row_count)return;
    lv_textarea_set_text(ssid,row_names[i]);lv_textarea_set_text(password,"");
    bool ok=false;
    if(saved_name(row_names[i]))ok=wifi_service_connect_saved(row_names[i]);
    else if(i<displayed.count && !displayed.secured[i])ok=wifi_service_connect(row_names[i],"");
    else { lv_keyboard_set_textarea(keyboard,password);lv_obj_remove_flag(keyboard,LV_OBJ_FLAG_HIDDEN);return; }
    lv_label_set_text(status,tr(NULL,ok?S_WIFI_PENDING:S_WIFI_REJECTED));
}
lv_obj_t *wifi_view_create(lv_obj_t *root) {
    revision=~0u;memset(&displayed,0,sizeof(displayed));
    panel=lv_obj_create(root);lv_obj_set_pos(panel,89,56);lv_obj_set_size(panel,705,347);
    lv_obj_set_style_pad_all(panel,10,0);lv_obj_remove_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel,lv_color_hex(0x14212b),0);
    lv_obj_set_style_text_font(panel,&tv4_ru_12,0);lv_obj_set_style_text_color(panel,lv_color_hex(0xffffff),0);
    lv_obj_set_style_border_color(panel,lv_color_hex(0x344752),0);
    text(panel,tr(NULL,S_WIFI_TITLE),0,0,660);status=text(panel,tr(NULL,S_WIFI_OFF),0,24,660);
    lv_label_set_long_mode(status,LV_LABEL_LONG_DOT);
    button(panel,tr(NULL,S_WIFI_SCAN),0,52,320,action,1);
    list=lv_obj_create(panel);lv_obj_set_pos(list,0,96);lv_obj_set_size(list,320,229);
    lv_obj_set_style_pad_all(list,0,0);lv_obj_set_style_border_width(list,0,0);
    lv_obj_set_style_bg_opa(list,LV_OPA_TRANSP,0);lv_obj_set_scroll_dir(list,LV_DIR_VER);
    text(panel,tr(NULL,S_WIFI_SSID),340,52,327);
    ssid=lv_textarea_create(panel);lv_obj_set_pos(ssid,340,72);lv_obj_set_size(ssid,327,38);
    lv_obj_set_style_bg_color(ssid,lv_color_hex(0x0b1720),0);lv_obj_set_style_text_color(ssid,lv_color_hex(0xffffff),0);
    lv_textarea_set_one_line(ssid,true);lv_textarea_set_max_length(ssid,32);
    text(panel,tr(NULL,S_WIFI_PASSWORD),340,119,327);
    password=lv_textarea_create(panel);lv_obj_set_pos(password,340,138);lv_obj_set_size(password,327,38);
    lv_obj_set_style_bg_color(password,lv_color_hex(0x0b1720),0);lv_obj_set_style_text_color(password,lv_color_hex(0xffffff),0);
    lv_textarea_set_one_line(password,true);lv_textarea_set_max_length(password,63);
    lv_textarea_set_password_mode(password,true);lv_textarea_set_password_bullet(password,"*");lv_textarea_set_password_show_time(password,0);
    lv_obj_add_event_cb(ssid,focus,LV_EVENT_FOCUSED,NULL);lv_obj_add_event_cb(password,focus,LV_EVENT_FOCUSED,NULL);
    button(panel,tr(NULL,S_CONNECT),340,188,158,action,2);
    button(panel,tr(NULL,S_WIFI_DISCONNECT),509,188,158,action,3);
    button(panel,tr(NULL,S_WIFI_FORGET),340,235,327,action,4);
    text(panel,tr(NULL,S_WIFI_SAVED_NOTE),340,284,327);
    for(unsigned i=0;i<WIFI_ROWS;i++) {
        networks[i]=button(list,"",0,(int)i*56,308,select_network,i);
        lv_obj_set_height(networks[i],52);lv_obj_add_flag(networks[i],LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(networks[i],lv_color_hex(0x233747),0);
        lv_obj_t *label=lv_obj_get_child(networks[i],0);lv_obj_set_size(label,282,32);
        lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_LEFT,0);
    }
    keyboard=lv_keyboard_create(panel);lv_obj_set_align(keyboard,LV_ALIGN_TOP_LEFT);lv_obj_set_pos(keyboard,0,180);lv_obj_set_size(keyboard,680,145);
    lv_obj_add_event_cb(keyboard,key_event,LV_EVENT_ALL,NULL);lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);
    return panel;
}
static void set_row(unsigned i,const char *name,const char *detail) {
    memcpy(row_names[i],name,strlen(name)+1);
    lv_label_set_text_fmt(lv_obj_get_child(networks[i],0),"%s\n%s",name,detail);
    lv_obj_remove_flag(networks[i],LV_OBJ_FLAG_HIDDEN);
}
void wifi_view_update(void) {
    wifi_snapshot_t snapshot;if(!wifi_service_snapshot(&snapshot))return;
    if(!lv_obj_has_flag(panel,LV_OBJ_FLAG_HIDDEN) && snapshot.state!=NET_OFF &&
       snapshot.state!=NET_SCANNING && snapshot.state!=NET_CONNECTING && lv_tick_elaps(last_scan)>=30000) {
        if(wifi_service_scan())last_scan=lv_tick_get();
    }
    if(revision==snapshot.revision)return;
    revision=snapshot.revision;displayed=snapshot;
    if(indicator) {
        uint32_t color=*snapshot.ip?0x42ed48:snapshot.state==NET_CONNECTING?0xffcb61:0x98b6ce;
        lv_obj_set_style_image_recolor(indicator,lv_color_hex(color),0);
        lv_obj_set_style_image_recolor_opa(indicator,LV_OPA_COVER,0);
    }
    static const string_id states[]={S_WIFI_OFF,S_WIFI_IDLE,S_WIFI_SCANNING,S_WIFI_CONNECTING,S_WIFI_ONLINE,S_WIFI_FAILED};
    char line[192];snprintf(line,sizeof(line),"%s · %s %s (%d)",
        tr(NULL,snapshot.storage_error?S_WIFI_SAVE_FAILED:states[snapshot.state]),snapshot.active_ssid,snapshot.ip,
        snapshot.storage_error?snapshot.storage_error:snapshot.error);
    lv_label_set_text(status,line);
    if(!*lv_textarea_get_text(ssid) && *snapshot.active_ssid)lv_textarea_set_text(ssid,snapshot.active_ssid);
    row_count=0;
    for(unsigned i=0;i<snapshot.count;i++) {
        string_id detail=snapshot.state==NET_ONLINE && !strcmp(snapshot.active_ssid,snapshot.ssids[i])?S_WIFI_ONLINE:
            saved_name(snapshot.ssids[i])?S_WIFI_SAVED:snapshot.secured[i]?S_WIFI_PROTECTED:S_WIFI_OPEN;
        snprintf(line,sizeof(line),"%s · %d dBm",tr(NULL,detail),snapshot.rssi[i]);
        set_row(row_count++,snapshot.ssids[i],line);
    }
    for(unsigned i=0;i<snapshot.saved_count;i++) {
        bool visible=false;for(unsigned j=0;j<snapshot.count;j++)if(!strcmp(snapshot.saved_ssids[i],snapshot.ssids[j]))visible=true;
        if(!visible)set_row(row_count++,snapshot.saved_ssids[i],tr(NULL,S_WIFI_UNAVAILABLE));
    }
    for(unsigned i=row_count;i<WIFI_ROWS;i++)lv_obj_add_flag(networks[i],LV_OBJ_FLAG_HIDDEN);
}
void wifi_view_hide(void) {
    lv_textarea_set_text(password,"");lv_keyboard_set_textarea(keyboard,NULL);lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);
}
void wifi_view_show(void) { last_scan=lv_tick_get()-30000;revision=~0u; }
