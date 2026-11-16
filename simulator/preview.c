#include "lvgl.h"
#include "dashboard.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "wifi_service.h"
#include "layout_checks.h"
static uint32_t pixels[800*480];
_Alignas(4) static uint8_t drawbuf[800*40*4];
static bool rgb565;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *map) {
    uint32_t *p=(uint32_t *)map; uint16_t *p16=(uint16_t *)map;
    for(int y=a->y1;y<=a->y2;y++) for(int x=a->x1;x<=a->x2;x++) {
        if(rgb565) {
            uint16_t v=*p16++;
            unsigned r=(v>>11)&31, g=(v>>5)&63, b=v&31;
            pixels[y*800+x]=((r*255/31)<<16)|((g*255/63)<<8)|(b*255/31);
        } else pixels[y*800+x]=*p++;
    }
    lv_display_flush_ready(d);
}
static lv_obj_t *find_button(lv_obj_t *root,const char *text) {
    if(lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN)) return NULL;
    if(lv_obj_check_type(root,&lv_button_class) && !strcmp(lv_label_get_text(lv_obj_get_child(root,0)),text)) return root;
    for(unsigned i=0;i<lv_obj_get_child_count(root);i++) {
        lv_obj_t *found=find_button(lv_obj_get_child(root,i),text);if(found)return found;
    }
    return NULL;
}
static void press_text(lv_obj_t *root,const char *text) {
    lv_obj_t *button=find_button(root,text);assert(button);
    lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);dashboard_update();check_dashboard_layout(root);
}
static void press(lv_obj_t *root,string_id id) { press_text(root,tr(NULL,id)); }
static lv_obj_t *find_class(lv_obj_t *root,const lv_obj_class_t *type,unsigned *skip) {
    if(lv_obj_check_type(root,type)) { if(!*skip)return root;--*skip; }
    for(unsigned i=0;i<lv_obj_get_child_count(root);i++) {
        lv_obj_t *found=find_class(lv_obj_get_child(root,i),type,skip);if(found)return found;
    }
    return NULL;
}
static bool has_label(lv_obj_t *root,const char *text) {
    if(lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN))return false;
    if(lv_obj_check_type(root,&lv_label_class) && !strcmp(lv_label_get_text(root),text))return true;
    for(unsigned i=0;i<lv_obj_get_child_count(root);i++)if(has_label(lv_obj_get_child(root,i),text))return true;
    return false;
}
static void save_frame(const char *name) {
    char path[80];snprintf(path,sizeof(path),"%s%s.ppm",name,rgb565?"-rgb565":"");
    lv_refr_now(lv_display_get_default());
    FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P6\n800 480\n255\n");
    for(unsigned i=0;i<800*480;i++) {
        unsigned char rgb[]={pixels[i]>>16,pixels[i]>>8,pixels[i]};assert(fwrite(rgb,1,3,file)==3);
    }
    fclose(file);
}
LV_FONT_DECLARE(tv4_ru_20);
LV_FONT_DECLARE(tv4_ru_12);
int main(int argc,char **argv) {
    assert(wifi_credentials_valid("bench","12345678"));
    assert(wifi_credentials_valid("open",""));
    assert(!wifi_credentials_valid("","12345678"));
    assert(!wifi_credentials_valid(NULL,"12345678"));
    assert(!wifi_credentials_valid("bench","short"));
    char long_ssid[34];memset(long_ssid,'a',33);long_ssid[33]=0;
    assert(!wifi_credentials_valid(long_ssid,"12345678"));
    char long_password[65];memset(long_password,'a',64);long_password[64]=0;
    assert(!wifi_credentials_valid("bench",long_password));
    long_password[63]=0;long_ssid[32]=0;
    assert(wifi_credentials_valid(long_ssid,long_password));
    rgb565=argc==2 && !strcmp(argv[1],"--rgb565");
    lv_init();
    /* Verify full RU alphabet, Ё/ё and required symbols are present in the real font. */
    lv_font_glyph_dsc_t g;
    for(unsigned c=0x410;c<=0x44f;c++) assert(lv_font_get_glyph_dsc(&tv4_ru_20,&g,c,0));
    unsigned extra[]={0x401,0x451,0xb1,0xb0,0xd7};
    for(unsigned i=0;i<sizeof(extra)/sizeof(*extra);i++) assert(lv_font_get_glyph_dsc(&tv4_ru_20,&g,extra[i],0));
    assert(lv_font_get_glyph_dsc(&tv4_ru_12,&g,0xd8,0));
    assert(lv_font_get_glyph_dsc(&tv4_ru_12,&g,0x2194,0));
    lv_display_t *d=lv_display_create(800,480);
    lv_display_set_color_format(d,rgb565?LV_COLOR_FORMAT_RGB565:LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(d,drawbuf,NULL,sizeof(drawbuf),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,flush);
    dashboard_create();
    for(int i=0;i<10;i++) { lv_tick_inc(20); dashboard_update(); lv_timer_handler(); }
    check_dashboard_layout(lv_screen_active());
    /* Long status variants must fit the same header slot as the initial label. */
    lv_obj_t *status_label=NULL;
    for(unsigned i=0;i<lv_obj_get_child_count(lv_screen_active());i++) {
        lv_obj_t *o=lv_obj_get_child(lv_screen_active(),i);
        if(lv_obj_check_type(o,&lv_label_class) && !strcmp(lv_label_get_text(o),tr(NULL,S_DEMO))) status_label=o;
    }
    assert(status_label);
    string_id statuses[]={S_DISCONNECTED,S_PAUSE,S_FAULT,S_DEMO};
    for(unsigned i=0;i<sizeof(statuses)/sizeof(*statuses);i++) {
        lv_label_set_text(status_label,tr(NULL,statuses[i])); check_dashboard_layout(lv_screen_active());
    }
    lv_refr_now(d);
    FILE *f=fopen(rgb565?"dashboard-rgb565.ppm":"dashboard.ppm","wb"); assert(f);
    fprintf(f,"P6\n800 480\n255\n");
    for(unsigned i=0;i<800*480;i++) {
        unsigned char rgb[]={pixels[i]>>16,pixels[i]>>8,pixels[i]};
        assert(fwrite(rgb,1,3,f)==3);
    }
    fclose(f);
    lv_obj_t *root=lv_screen_active();
    press_text(root,"+"); assert(dashboard_snapshot().rpm==1300);
    press_text(root,"−"); assert(dashboard_snapshot().rpm==1250);
    lv_obj_t *override=NULL;
    for(unsigned i=0;i<lv_obj_get_child_count(root);i++) {
        lv_obj_t *child=lv_obj_get_child(root,i);
        if(lv_obj_check_type(child,&lv_slider_class)) override=child;
    }
    assert(override);
    /* A burst of drag updates is coalesced rather than overflowing the event queue. */
    for(int i=0;i<=150;i++) {
        lv_slider_set_value(override,i,LV_ANIM_OFF);
        lv_obj_send_event(override,LV_EVENT_VALUE_CHANGED,NULL);
    }
    dashboard_update();
    assert(dashboard_snapshot().mode==SIMULATION && dashboard_snapshot().feed==270);
    check_dashboard_layout(root);save_frame("controls");
    lv_slider_set_value(override,100,LV_ANIM_OFF);
    lv_obj_send_event(override,LV_EVENT_VALUE_CHANGED,NULL);dashboard_update();
    assert(dashboard_snapshot().feed==180);
    wifi_snapshot_t fixture={.state=NET_ONLINE,.revision=1,.count=8,.saved_count=2};
    strcpy(fixture.active_ssid,"TEST_HOME");strcpy(fixture.ip,"192.0.2.10");
    strcpy(fixture.saved_ssids[0],"TEST_HOME");strcpy(fixture.saved_ssids[1],"TEST_OFFLINE");
    for(unsigned i=0;i<fixture.count;i++) {
        snprintf(fixture.ssids[i],33,"TEST_NETWORK_%u",i);fixture.rssi[i]=(int8_t)(-35-(int)i*5);fixture.secured[i]=true;
    }
    strcpy(fixture.ssids[0],"TEST_HOME");wifi_service_host_fixture(&fixture);
    assert(dashboard_set_view(VIEW_WIFI));dashboard_update();
    check_dashboard_layout(root);save_frame("wifi");
    press(root,S_WIFI_SCAN); /* Host backend cannot scan or connect. */
    assert(has_label(root,tr(NULL,S_WIFI_REJECTED)));
    /* Saved entries are selectable; host still refuses real network activity. */
    assert(has_label(root,"TEST_HOME\nПодключено · -35 dBm"));
    lv_obj_t *saved_row=find_button(root,"TEST_OFFLINE\nВне зоны сети");assert(saved_row);
    lv_obj_scroll_to_view(saved_row,LV_ANIM_OFF);check_dashboard_layout(root);
    save_frame("wifi-list-bottom");
    press_text(root,"TEST_OFFLINE\nВне зоны сети");
    unsigned skip=0;lv_obj_t *wifi_keyboard=find_class(root,&lv_keyboard_class,&skip);
    assert(wifi_keyboard);
    lv_obj_t *wifi_parent=lv_obj_get_parent(wifi_keyboard);skip=1;
    lv_obj_t *wifi_password=find_class(wifi_parent,&lv_textarea_class,&skip);assert(wifi_password);
    lv_textarea_set_text(wifi_password,"testonly123");
    lv_obj_send_event(wifi_password,LV_EVENT_FOCUSED,NULL);
    assert(!lv_obj_has_flag(wifi_keyboard,LV_OBJ_FLAG_HIDDEN));
    save_frame("wifi-keyboard");
    assert(dashboard_set_view(VIEW_HOME));dashboard_update();
    assert(!*lv_textarea_get_text(wifi_password));
    assert(lv_obj_has_flag(wifi_keyboard,LV_OBJ_FLAG_HIDDEN));
    wifi_service_host_fixture(NULL);
    lv_obj_t *step=NULL, *ack=NULL;
    for(unsigned i=0;i<lv_obj_get_child_count(root);i++) {
        lv_obj_t *child=lv_obj_get_child(root,i);
        if(!lv_obj_check_type(child,&lv_button_class)) continue;
        const char *text=lv_label_get_text(lv_obj_get_child(child,0));
        if(!strcmp(text,tr(NULL,S_X_PLUS))) step=child;
        if(!strcmp(text,tr(NULL,S_RESET))) ack=child;
    }
    assert(step && ack);
    lv_obj_send_event(step,LV_EVENT_CLICKED,NULL); dashboard_update();
    assert(dashboard_snapshot().x_um==25440);
    assert(dashboard_snapshot().rpm==1250);
    for(int i=0;i<20;i++) lv_obj_send_event(step,LV_EVENT_CLICKED,NULL);
    dashboard_update(); assert(dashboard_snapshot().mode==FAULT);
    assert(dashboard_snapshot().rpm==0 && dashboard_snapshot().feed==0);
    check_dashboard_layout(root);
    lv_obj_send_event(ack,LV_EVENT_CLICKED,NULL); dashboard_update();
    assert(dashboard_snapshot().mode==HOLD);
    check_dashboard_layout(root);
    int32_t x=dashboard_snapshot().x_um;
    lv_obj_send_event(step,LV_EVENT_CLICKED,NULL); dashboard_update();
    assert(dashboard_snapshot().x_um==x);
    assert(dashboard_post_event(EV_EMERGENCY)); dashboard_update();
    assert(dashboard_snapshot().mode==FAULT);
    assert(dashboard_post_event(EV_ACK)); dashboard_update();
    assert(dashboard_snapshot().mode==HOLD);
    assert(!dashboard_post_event((event_t)255)); dashboard_update();
    assert(dashboard_snapshot().mode==FAULT);
    press(root,S_DIAG); assert(dashboard_get_view()==VIEW_DIAGNOSTICS);
    press(root,S_TEST_TOUCH); assert(dashboard_snapshot().fault==F_TOUCH);
    press(root,S_NEW_SIM); assert(dashboard_snapshot().mode==FAULT);
    save_frame("diagnostics");
    press(root,S_ACK); assert(dashboard_snapshot().mode==HOLD);
    press(root,S_NEW_SIM); assert(dashboard_snapshot().mode==SIMULATION);
    press(root,S_TEST_CONNECT); assert(dashboard_snapshot().connected);
    lv_tick_inc(500);dashboard_update();check_dashboard_layout(root);
    assert(dashboard_snapshot().mode==DISCONNECTED);
    press(root,S_ACK);press(root,S_NEW_SIM);
    press(root,S_TEST_DISPLAY);assert(dashboard_snapshot().fault==F_DISPLAY);
    press(root,S_ACK);press(root,S_NEW_SIM);
    for(unsigned i=0;i<70;i++) { assert(dashboard_post_event(EV_INC_10));dashboard_update(); }
    event_t sample_events[]={EV_EMERGENCY,EV_ACK,EV_NEW_SIM,EV_STOP,EV_ACK,EV_NEW_SIM,EV_INC_100,EV_INC_10};
    for(unsigned i=0;i<sizeof(sample_events)/sizeof(*sample_events);i++) {
        lv_tick_inc(50);assert(dashboard_post_event(sample_events[i]));dashboard_update();
    }
    check_dashboard_layout(root);
    press(root,S_LOG); assert(dashboard_get_view()==VIEW_LOG);
    save_frame("journal");
    for(int i=0;i<10;i++)press(root,S_NEXT); /* Clamps to oldest retained page. */
    for(int i=0;i<10;i++)press(root,S_PREVIOUS);
    assert(!dashboard_set_view((dashboard_view_t)99));
    press(root,S_HOME); assert(dashboard_get_view()==VIEW_HOME);
    press(root,S_X_PLUS); assert(dashboard_snapshot().x_um==25440);
    int32_t tool_test_x=dashboard_snapshot().x_um,tool_test_z=dashboard_snapshot().z_um;
    press(root,S_TOOLS);assert(dashboard_get_view()==VIEW_TOOLS);
    char second_tool[80];snprintf(second_tool,sizeof(second_tool),tr(NULL,S_FMT_TOOL_ROW),2,tr(NULL,S_TOOL_SHORT_2));
    press(root,S_TOOL_DEC_XZ);assert(find_button(root,"-0,100"));press(root,S_TOOL_INC_XZ);
    press_text(root,"0,000");press_text(root,"9");press(root,S_CANCEL);
    assert(find_button(root,"0,000"));
    press_text(root,"0,000");press_text(root,"1");press_text(root,",");press_text(root,"2");press_text(root,"5");press_text(root,"0");press(root,S_ACCEPT);
    assert(dashboard_snapshot().tools.geometry[0].x_um==0); /* Draft only. */
    press_text(root,second_tool);assert(find_button(root,"1,250")); /* Dirty row cannot be discarded. */
    press(root,S_ACTIVATE);assert(dashboard_snapshot().tool==1);
    press(root,S_CANCEL); /* Discard draft. */
    press_text(root,"0,000");press_text(root,"±");press_text(root,"1");press_text(root,",");press_text(root,"2");press_text(root,"5");press_text(root,"0");press(root,S_ACCEPT);press(root,S_SAVE);
    assert(dashboard_snapshot().tools.geometry[0].x_um==-1250);
    press_text(root,second_tool);
    press_text(root,"0,400");press_text(root,"1");press_text(root,",");press_text(root,"2");press_text(root,",");press_text(root,"3");press(root,S_ACCEPT);
    assert(find_button(root,tr(NULL,S_ACCEPT))); /* Invalid number keeps keypad open. */
    press(root,S_CLEAR);press_text(root,"0");press_text(root,",");press_text(root,"8");press_text(root,"0");press_text(root,"0");
    save_frame("tools-keypad");press(root,S_ACCEPT);
    assert(dashboard_post_event(EV_TOUCH_FAULT));dashboard_update();press(root,S_SAVE);
    assert(dashboard_snapshot().tools.geometry[1].radius_um==400);
    assert(dashboard_post_event(EV_ACK));dashboard_update();press(root,S_SAVE);press(root,S_ACTIVATE);
    assert(dashboard_snapshot().tool==2 && dashboard_snapshot().tools.geometry[1].radius_um==800);
    assert(dashboard_snapshot().x_um==tool_test_x && dashboard_snapshot().z_um==tool_test_z);
    save_frame("tools");press(root,S_HOME);check_dashboard_layout(root);
    assert(has_label(root,"T02") && has_label(root,"0.800"));
    puts("LVGL 9 render and Cyrillic coverage passed"); return 0;
}
