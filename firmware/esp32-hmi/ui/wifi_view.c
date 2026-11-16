#include "wifi_view.h"
#include "wifi_service.h"
#include "strings.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(tv4_ru_12);
static lv_obj_t *panel,*status,*ssid,*password,*keyboard,*networks[WIFI_UI_NETWORKS];
static wifi_snapshot_t displayed;
static unsigned revision=~0u;
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
    if(kind==1)ok=wifi_service_scan();
    if(kind==2) { ok=wifi_service_connect(lv_textarea_get_text(ssid),lv_textarea_get_text(password));lv_textarea_set_text(password,""); }
    if(kind==3)ok=wifi_service_disconnect();
    lv_label_set_text(status,tr(NULL,ok?S_WIFI_PENDING:S_WIFI_REJECTED));
}
static lv_obj_t *button(lv_obj_t *p,const char *s,int x,int y,int w,lv_event_cb_t cb,uintptr_t data) {
    lv_obj_t *b=lv_button_create(p);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,36);
    lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,s);lv_obj_center(l);
    lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,(void *)data);return b;
}
static void select_network(lv_event_t *e) {
    unsigned i=(unsigned)(uintptr_t)lv_event_get_user_data(e);
    if(i<displayed.count)lv_textarea_set_text(ssid,displayed.ssids[i]);
}
lv_obj_t *wifi_view_create(lv_obj_t *root) {
    revision=~0u;
    panel=lv_obj_create(root);lv_obj_set_pos(panel,89,56);lv_obj_set_size(panel,705,347);
    lv_obj_set_style_pad_all(panel,10,0);lv_obj_remove_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel,lv_color_hex(0x14212b),0);
    lv_obj_set_style_text_font(panel,&tv4_ru_12,0);
    lv_obj_set_style_text_color(panel,lv_color_hex(0xffffff),0);
    lv_obj_set_style_border_color(panel,lv_color_hex(0x344752),0);
    text(panel,tr(NULL,S_WIFI_TITLE),0,0,660);
    status=text(panel,tr(NULL,S_WIFI_OFF),0,24,660);
    button(panel,tr(NULL,S_WIFI_SCAN),0,52,200,action,1);
    text(panel,tr(NULL,S_WIFI_SSID),232,52,420);
    ssid=lv_textarea_create(panel);lv_obj_set_pos(ssid,232,72);lv_obj_set_size(ssid,435,38);
    lv_obj_set_style_bg_color(ssid,lv_color_hex(0x0b1720),0);
    lv_obj_set_style_text_color(ssid,lv_color_hex(0xffffff),0);
    lv_textarea_set_one_line(ssid,true);lv_textarea_set_max_length(ssid,32);
    text(panel,tr(NULL,S_WIFI_PASSWORD),232,119,430);
    password=lv_textarea_create(panel);lv_obj_set_pos(password,232,138);lv_obj_set_size(password,435,38);
    lv_obj_set_style_bg_color(password,lv_color_hex(0x0b1720),0);
    lv_obj_set_style_text_color(password,lv_color_hex(0xffffff),0);
    lv_textarea_set_one_line(password,true);lv_textarea_set_max_length(password,63);
    lv_textarea_set_password_mode(password,true);lv_textarea_set_password_bullet(password,"*");
    lv_textarea_set_password_show_time(password,0);
    lv_obj_add_event_cb(ssid,focus,LV_EVENT_FOCUSED,NULL);lv_obj_add_event_cb(password,focus,LV_EVENT_FOCUSED,NULL);
    button(panel,tr(NULL,S_CONNECT),232,188,208,action,2);
    button(panel,tr(NULL,S_WIFI_DISCONNECT),453,188,214,action,3);
    text(panel,tr(NULL,S_WIFI_NOTE),232,239,430);
    for(unsigned i=0;i<WIFI_UI_NETWORKS;i++) {
        networks[i]=button(panel,"",0,96+(int)i*37,210,select_network,i);
        lv_obj_add_flag(networks[i],LV_OBJ_FLAG_HIDDEN);
    }
    /* Keyboard lives inside the view so navigating away hides it too. */
    keyboard=lv_keyboard_create(panel);lv_obj_set_align(keyboard,LV_ALIGN_TOP_LEFT);lv_obj_set_pos(keyboard,0,180);lv_obj_set_size(keyboard,680,145);
    lv_obj_add_event_cb(keyboard,key_event,LV_EVENT_ALL,NULL);lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);
    return panel;
}
void wifi_view_update(void) {
    wifi_snapshot_t snapshot;if(!wifi_service_snapshot(&snapshot) || revision==snapshot.revision)return;
    revision=snapshot.revision;displayed=snapshot;
    static const string_id states[]={S_WIFI_OFF,S_WIFI_IDLE,S_WIFI_SCANNING,S_WIFI_CONNECTING,S_WIFI_ONLINE,S_WIFI_FAILED};
    char line[160];snprintf(line,sizeof(line),"%s %s (%d)",tr(NULL,states[snapshot.state]),snapshot.ip,snapshot.error);
    lv_label_set_text(status,line);
    for(unsigned i=0;i<WIFI_UI_NETWORKS;i++) {
        if(i<snapshot.count) {
            snprintf(line,sizeof(line),"%.32s  %d dBm",snapshot.ssids[i],snapshot.rssi[i]);
            lv_label_set_text(lv_obj_get_child(networks[i],0),line);
            lv_obj_set_width(lv_obj_get_child(networks[i],0),186);
            lv_label_set_long_mode(lv_obj_get_child(networks[i],0),LV_LABEL_LONG_DOT);
            lv_obj_remove_flag(networks[i],LV_OBJ_FLAG_HIDDEN);
        } else lv_obj_add_flag(networks[i],LV_OBJ_FLAG_HIDDEN);
    }
}

void wifi_view_hide(void) {
    lv_textarea_set_text(password,"");
    lv_keyboard_set_textarea(keyboard,NULL);
    lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);
}
