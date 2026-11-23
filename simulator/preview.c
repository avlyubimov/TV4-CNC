#include "lvgl.h"
#include "dashboard.h"
#include "boot_view.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "wifi_service.h"
#include "wifi_editor.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
#include "layout_checks.h"
#include "firmware_view.h"
static bool ota_fixture;
static tv4_update_snapshot ota_sample;
static unsigned ota_posts;static tv4_update_command ota_posted;
void __real_tv4_ota_service_snapshot(tv4_update_snapshot *);
bool __real_tv4_ota_service_request(tv4_update_command);
void __wrap_tv4_ota_service_snapshot(tv4_update_snapshot *out) {
    if(ota_fixture)*out=ota_sample;else __real_tv4_ota_service_snapshot(out);
}
bool __wrap_tv4_ota_service_request(tv4_update_command cmd) {
    if(!ota_fixture)return __real_tv4_ota_service_request(cmd);
    ota_posts++;ota_posted=cmd;return true;
}
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
static void keyboard_press(lv_obj_t *keyboard,const char *key) {
    for(unsigned i=0;i<80;i++) {
        const char *text=lv_keyboard_get_button_text(keyboard,i);if(!text)break;
        if(!strcmp(text,key)) { lv_buttonmatrix_set_selected_button(keyboard,i);lv_obj_send_event(keyboard,LV_EVENT_VALUE_CHANGED,NULL);return; }
    }
    assert(false);
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
static void firmware_navigation(lv_obj_t *root,lv_obj_t *wifi_password) {
    unsigned children=lv_obj_get_child_count(root);
    model_t before=dashboard_snapshot();
    firmware_info_t info;
    const char build[]="host-preview",sdk[]="test-sdk";
    assert(firmware_info_set(&info,build,sizeof(build),sdk,sizeof(sdk),
        FIRMWARE_INFO_SIMULATOR,FIRMWARE_PROFILE_REV12));
    dashboard_set_firmware_info(&info);info.build[0]='X';
    press(root,S_SETTINGS);assert(dashboard_get_view()==VIEW_SETTINGS);
    assert(find_button(root,tr(NULL,S_SETTINGS_WIFI)) && find_button(root,tr(NULL,S_FW_TITLE)));
    save_frame("settings");
    press(root,S_FW_TITLE);assert(dashboard_get_view()==VIEW_FIRMWARE);
    assert(has_label(root,build) && has_label(root,sdk));
    assert(has_label(root,tr(NULL,S_FW_SIMULATOR)) && has_label(root,tr(NULL,S_FW_PEER_UNKNOWN)));
    assert(has_label(root,tr(NULL,S_FW_UPDATE_OFF)));
    assert(find_button(root,tr(NULL,S_FW_BACK)));
    /* The only controls on the read-only page are navigation and the existing
     * persistent STOP; no fake check/download/install action is created. */
    save_frame("firmware-info");
    dashboard_set_firmware_info(NULL);check_dashboard_layout(root);
    assert(has_label(root,tr(NULL,S_FW_UNKNOWN)) && !has_label(root,build));
    save_frame("firmware-info-unknown");
    char longest[32];memset(longest,'W',31);longest[31]=0;
    for(unsigned profile=FIRMWARE_PROFILE_HEADLESS;profile<=FIRMWARE_PROFILE_OTA;profile++) {
        assert(firmware_info_set(&info,longest,sizeof(longest),longest,sizeof(longest),
            FIRMWARE_INFO_SIMULATOR,(firmware_info_profile_t)profile));
        dashboard_set_firmware_info(&info);check_dashboard_layout(root);
        assert(has_label(root,longest));
    }
    save_frame("firmware-info-long");
    memset(info.sdk,'W',sizeof(info.sdk));dashboard_set_firmware_info(&info);
    assert(!has_label(root,longest) && has_label(root,tr(NULL,S_FW_UNKNOWN)));
    assert(firmware_info_set(&info,build,sizeof(build),sdk,sizeof(sdk),
        FIRMWARE_INFO_SIMULATOR,FIRMWARE_PROFILE_REV12));dashboard_set_firmware_info(&info);
    press(root,S_FW_BACK);assert(dashboard_get_view()==VIEW_SETTINGS);
    press(root,S_SETTINGS_WIFI);assert(dashboard_get_view()==VIEW_WIFI);
    lv_textarea_set_text(wifi_password,"sensitive-test-only");
    lv_obj_send_event(wifi_password,LV_EVENT_FOCUSED,NULL);assert(wifi_editor_is_open());
    press(root,S_SETTINGS);assert(!wifi_editor_is_open() && !*lv_textarea_get_text(wifi_password));
    press(root,S_FW_TITLE);assert(!has_label(root,"sensitive-test-only"));
    press(root,S_HOME);assert(lv_obj_get_child_count(root)==children);
    /* Warmed LVGL builtin-pool footprint must return exactly after navigation;
     * this is a host allocation regression, not ESP32 heap/latency evidence. */
    lv_mem_monitor_t memory_before,memory_after;lv_mem_monitor(&memory_before);
    for(unsigned i=0;i<64;i++) {
        press(root,S_SETTINGS);assert(lv_obj_get_child_count(root)==children+1);
        press(root,S_FW_TITLE);assert(lv_obj_get_child_count(root)==children+1);
        press(root,S_FW_BACK);assert(dashboard_get_view()==VIEW_SETTINGS);
        press(root,S_HOME);assert(lv_obj_get_child_count(root)==children);
    }
    lv_mem_monitor(&memory_after);assert(memory_after.free_size==memory_before.free_size);
    model_t after=dashboard_snapshot();
    assert(after.mode==before.mode && after.connected==before.connected);
    assert(after.x_um==before.x_um && after.z_um==before.z_um && after.rpm==before.rpm);
    press(root,S_SETTINGS);press(root,S_FW_TITLE);
    assert(dashboard_post_event(EV_TOUCH_FAULT));dashboard_update();
    assert(dashboard_snapshot().mode==FAULT && find_button(root,tr(NULL,S_STOP)));
    save_frame("firmware-info-fault");
    press(root,S_STOP);assert(dashboard_snapshot().mode==FAULT);
    press(root,S_FW_BACK);press(root,S_HOME);assert(dashboard_snapshot().mode==FAULT);
    assert(!dashboard_set_view((dashboard_view_t)(VIEW_FIRMWARE+1)));
    puts("Firmware UI PASS: local/simulator/unknown/31-byte/profile states, Wi-Fi editor cleanup, "
        "64 navigation cycles with identical host-pool free bytes, persistent STOP/fault/no motion");
}
static void ota_preview(lv_obj_t *original) {
    /* Explicit SIMULATOR fixture, not a claim of running OTA or device health. */
    lv_obj_t *screen=lv_obj_create(NULL);lv_screen_load(screen);
    firmware_view_t view={0};firmware_view_create(&view,screen,NULL,NULL);
    firmware_info_t info;const char build[]="host-fixture-only",sdk[]="v6.1.1";
    assert(firmware_info_set(&info,build,sizeof(build),sdk,sizeof(sdk),FIRMWARE_INFO_SIMULATOR,FIRMWARE_PROFILE_OTA));
    firmware_view_update(&view,&info);ota_fixture=true;ota_posts=0;
    ota_sample=(tv4_update_snapshot){.enabled=true,.install_allowed=true,.total=3145728,.received=1048576};
    strcpy(ota_sample.version,"4294967295.4294967295.4294967295");
    for(unsigned state=TV4_UPDATE_SETUP;state<=TV4_UPDATE_BOOT_FAILED;state++) {
        ota_sample.state=(tv4_update_state)state;firmware_view_ota_update(&view,&ota_sample);
        check_dashboard_layout(screen);
        bool idle=state==TV4_UPDATE_IDLE || state==TV4_UPDATE_READY || state==TV4_UPDATE_ERROR;
        bool install=state==TV4_UPDATE_READY || state==TV4_UPDATE_RESTART;
        assert(lv_obj_has_state(view.action[0],LV_STATE_DISABLED)==!idle);
        assert(lv_obj_has_state(view.action[1],LV_STATE_DISABLED)==!install);
        if(state==TV4_UPDATE_SETUP)save_frame("ota-unconfigured");
        if(state==TV4_UPDATE_WRITING)save_frame("ota-writing-fixture");
    }
    ota_sample.state=TV4_UPDATE_READY;firmware_view_ota_update(&view,&ota_sample);
    lv_obj_send_event(view.action[1],LV_EVENT_CLICKED,NULL);firmware_view_ota_update(&view,&ota_sample);
    assert(view.confirm && !ota_posts);check_dashboard_layout(screen);save_frame("ota-confirm-fixture");
    ota_sample.install_allowed=false;firmware_view_ota_update(&view,&ota_sample);
    assert(!view.confirm && lv_obj_has_state(view.action[1],LV_STATE_DISABLED));
    assert(!strcmp(lv_label_get_text(view.caption[1]),tr(NULL,S_OTA_CHECK_ONLY)));
    lv_obj_send_event(view.action[1],LV_EVENT_CLICKED,NULL);
    lv_obj_send_event(view.action[1],LV_EVENT_CLICKED,NULL);firmware_view_ota_update(&view,&ota_sample);
    assert(!view.confirm && !ota_posts);check_dashboard_layout(screen);save_frame("ota-check-only-fixture");
    ota_sample.install_allowed=true;firmware_view_ota_update(&view,&ota_sample);
    lv_obj_send_event(view.action[1],LV_EVENT_CLICKED,NULL);firmware_view_ota_update(&view,&ota_sample);
    assert(view.confirm && !ota_posts);
    lv_obj_send_event(view.action[2],LV_EVENT_CLICKED,NULL);firmware_view_ota_update(&view,&ota_sample);
    assert(!view.confirm && !ota_posts);
    lv_obj_send_event(view.action[1],LV_EVENT_CLICKED,NULL);firmware_view_ota_update(&view,&ota_sample);
    ota_sample.sha256[0]^=1;firmware_view_ota_update(&view,&ota_sample);assert(!view.confirm);
    lv_obj_send_event(view.action[1],LV_EVENT_CLICKED,NULL);firmware_view_ota_update(&view,&ota_sample);
    lv_obj_send_event(view.action[1],LV_EVENT_CLICKED,NULL);assert(ota_posts==1 && ota_posted==TV4_UPDATE_INSTALL);
    ota_sample.state=TV4_UPDATE_WRITING;firmware_view_ota_update(&view,&ota_sample);
    lv_obj_send_event(view.action[2],LV_EVENT_CLICKED,NULL);assert(ota_posted==TV4_UPDATE_CANCEL);
    ota_sample.state=TV4_UPDATE_RESTART;ota_sample.install_allowed=false;firmware_view_ota_update(&view,&ota_sample);
    assert(!lv_obj_has_state(view.action[1],LV_STATE_DISABLED));
    lv_obj_send_event(view.action[1],LV_EVENT_CLICKED,NULL);assert(ota_posted==TV4_UPDATE_REBOOT);
    firmware_view_destroy(&view);assert(!view.panel && !view.confirm);ota_fixture=false;
    lv_screen_load(original);lv_obj_delete(screen);
}
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
    boot_view_create();check_dashboard_layout(lv_screen_active());save_frame("boot-start");
    assert(!boot_view_finish());
    boot_view_stage(BOOT_TOUCH);check_dashboard_layout(lv_screen_active());save_frame("boot-touch");
    boot_view_fail(S_BOOT_TOUCH_FAILED,"ESP_ERR_TIMEOUT");
    boot_view_stage(BOOT_READY);assert(!boot_view_finish());
    check_dashboard_layout(lv_screen_active());save_frame("boot-error");
    assert(has_label(lv_screen_active(),tr(NULL,S_BOOT_FAILED)));
    /* A reboot creates a fresh screen; a failed startup cannot dismiss itself. */
    lv_obj_clean(lv_screen_active());boot_view_create();
    boot_view_stage(BOOT_TOUCH);boot_view_stage(BOOT_NETWORK);boot_view_stage(BOOT_INTERFACE);
    boot_view_stage(BOOT_READY);check_dashboard_layout(lv_screen_active());save_frame("boot-ready");
    /* Match app_main: create the dashboard behind the splash, then remove it. */
    dashboard_create();boot_view_stage(BOOT_READY);lv_refr_now(d);
    assert(boot_view_finish());
    check_dashboard_layout(lv_screen_active());
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
    /* The per-axis target also requires an explicit confirmation. */
    lv_obj_t *x_target=NULL;
    for(unsigned i=0;i<lv_obj_get_child_count(root);i++) {
        lv_obj_t *o=lv_obj_get_child(root,i);
        if(lv_obj_get_width(o)==343 && lv_obj_get_height(o)==59 && has_label(o,tr(NULL,S_X)))x_target=lv_obj_get_child(o,-1);
    }
    assert(x_target);lv_obj_send_event(x_target,LV_EVENT_CLICKED,NULL);dashboard_update();
    assert(has_label(lv_layer_top(),tr(NULL,S_ZERO_WORK_X)));
    press(lv_layer_top(),S_CANCEL);
    int32_t machine_x=dashboard_snapshot().x_um,machine_z=dashboard_snapshot().z_um;
    press(root,S_WORK_ZERO);
    check_dashboard_layout(lv_layer_top());save_frame("coordinate-confirm");
    assert(!dashboard_snapshot().work_frame);press(lv_layer_top(),S_CANCEL);
    assert(!dashboard_snapshot().work_frame && !lv_obj_get_child_count(lv_layer_top()));
    press(root,S_WORK_ZERO);press(lv_layer_top(),S_ACCEPT);
    model_t datum=dashboard_snapshot();assert(datum.work_frame);
    assert(datum.x_um==machine_x && datum.z_um==machine_z && model_display_x(&datum)==0 && model_display_z(&datum)==0);
    save_frame("coordinates-work");
    /* Switch readout without moving the virtual axes or changing the datum. */
    lv_obj_t *frame_label=NULL;
    for(unsigned i=0;i<lv_obj_get_child_count(root);i++) {
        lv_obj_t *o=lv_obj_get_child(root,i);
        if(lv_obj_check_type(o,&lv_label_class) && !strcmp(lv_label_get_text(o),tr(NULL,S_WORK_FRAME)))frame_label=o;
    }
    assert(frame_label);lv_obj_send_event(frame_label,LV_EVENT_CLICKED,NULL);dashboard_update();
    assert(!dashboard_snapshot().work_frame && dashboard_snapshot().x_um==machine_x);
    press(root,S_MACHINE_TAG);assert(dashboard_snapshot().work_frame);
    press(root,S_WORK_TAG);assert(!dashboard_snapshot().work_frame);
    /* A stop while the confirmation is open closes it and keeps the datum. */
    press(root,S_WORK_ZERO);assert(lv_obj_get_child_count(lv_layer_top())==1);
    press(root,S_STOP);assert(!lv_obj_get_child_count(lv_layer_top()));
    assert(dashboard_snapshot().mode==HOLD && dashboard_snapshot().work_x_um==datum.work_x_um);
    assert(dashboard_post_event(EV_NEW_SIM));dashboard_update();
    assert(dashboard_post_event(EV_INC_1000));dashboard_update();
    for(unsigned i=0;i<974;i++) { assert(dashboard_post_event(EV_STEP));dashboard_update(); }
    press(root,S_WORK_ZERO);press(lv_layer_top(),S_ACCEPT);
    for(unsigned i=0;i<1999;i++) { assert(dashboard_post_event(EV_X_MINUS));dashboard_update(); }
    model_t wide=dashboard_snapshot();assert(model_display_x(&wide)==-1999000);
    check_dashboard_layout(root);save_frame("coordinates-range");
    assert(dashboard_post_event(EV_STOP));dashboard_update();
    assert(dashboard_post_event(EV_NEW_SIM));assert(dashboard_post_event(EV_INC_10));dashboard_update();
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
    lv_obj_t *wifi_parent=lv_obj_get_parent(lv_obj_get_parent(saved_row));unsigned skip=1;
    lv_obj_t *wifi_password=find_class(wifi_parent,&lv_textarea_class,&skip);assert(wifi_password);
    lv_textarea_set_text(wifi_password,"testonly123");lv_obj_send_event(wifi_password,LV_EVENT_FOCUSED,NULL);
    assert(wifi_editor_is_open());skip=0;
    lv_obj_t *wifi_keyboard=find_class(lv_layer_top(),&lv_keyboard_class,&skip);assert(wifi_keyboard);
    skip=0;lv_obj_t *editor_input=find_class(lv_layer_top(),&lv_textarea_class,&skip);assert(editor_input);
    assert(lv_textarea_get_password_mode(editor_input));
    lv_obj_update_layout(lv_layer_top());lv_buttonmatrix_t *matrix=(lv_buttonmatrix_t *)wifi_keyboard;
    for(unsigned i=0;i<matrix->btn_cnt;i++) {
        assert(matrix->button_areas[i].x2-matrix->button_areas[i].x1+1>=44);
        assert(matrix->button_areas[i].y2-matrix->button_areas[i].y1+1>=44);
    }
    save_frame("wifi-keyboard");
    press(lv_layer_top(),S_INPUT_SHOW);assert(!lv_textarea_get_password_mode(editor_input));
    save_frame("wifi-password-visible");
    press(lv_layer_top(),S_INPUT_HIDE);assert(lv_textarea_get_password_mode(editor_input));
    lv_textarea_set_text(editor_input,"");
    keyboard_press(wifi_keyboard,"1");keyboard_press(wifi_keyboard,"ABC");keyboard_press(wifi_keyboard,"A");
    keyboard_press(wifi_keyboard,"abc");keyboard_press(wifi_keyboard,"a");keyboard_press(wifi_keyboard,"1#");
    keyboard_press(wifi_keyboard,"@");keyboard_press(wifi_keyboard,"2");keyboard_press(wifi_keyboard,"3");
    keyboard_press(wifi_keyboard,"4");keyboard_press(wifi_keyboard,"5");
    assert(!strcmp(lv_textarea_get_text(editor_input),"1Aa@2345"));
    press(lv_layer_top(),S_CANCEL);assert(!wifi_editor_is_open());assert(!strcmp(lv_textarea_get_text(wifi_password),"testonly123"));
    lv_obj_send_event(wifi_password,LV_EVENT_FOCUSED,NULL);skip=0;
    editor_input=find_class(lv_layer_top(),&lv_textarea_class,&skip);
    lv_textarea_set_text(editor_input,"1Aa@2345");press(lv_layer_top(),S_INPUT_DONE);
    assert(!wifi_editor_is_open() && !strcmp(lv_textarea_get_text(wifi_password),"1Aa@2345"));
    press(root,S_CONNECT);assert(!strcmp(lv_textarea_get_text(wifi_password),"1Aa@2345"));
    fixture.revision++;fixture.state=NET_FAILED;fixture.error=263;fixture.ip[0]=0;
    wifi_service_host_fixture(&fixture);dashboard_update();
    char timeout_text[256];snprintf(timeout_text,sizeof(timeout_text),tr(NULL,S_WIFI_CODE),tr(NULL,S_WIFI_ERR_TIMEOUT),263);
    assert(has_label(root,timeout_text));save_frame("wifi-timeout");
    assert(dashboard_set_view(VIEW_HOME));dashboard_update();assert(!*lv_textarea_get_text(wifi_password));
    assert(!wifi_editor_is_open());
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
    press(root,S_RESOURCES);
    diag_record_t *metrics=&dashboard_diagnostics()->record;
    metrics->resources_valid=true;metrics->internal_free=220000;metrics->internal_min=180000;
    metrics->internal_largest=160000;metrics->psram_total=8388608;metrics->psram_free=6200000;
    metrics->psram_min=5900000;metrics->owner_stack=2048;metrics->touch_stack=1900;metrics->wifi_stack=3000;
    lv_tick_inc(1000);dashboard_update();check_dashboard_layout(root);save_frame("diagnostics-resources");
    press(root,S_RESOURCES);
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
    firmware_navigation(root,wifi_password);
    ota_preview(root);
    puts("LVGL 9 render and Cyrillic coverage passed"); return 0;
}
