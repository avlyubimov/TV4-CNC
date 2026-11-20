#include "firmware_view.h"
#include "strings.h"
#include <string.h>
LV_FONT_DECLARE(tv4_ru_12);
LV_FONT_DECLARE(tv4_ru_14);
LV_FONT_DECLARE(tv4_ru_20);

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
    text(view->panel,tr(NULL,S_FW_UPDATE_OFF),20,266,665,&tv4_ru_14,0xffcb61);
    text(view->panel,tr(NULL,S_FW_UPDATE_NOTE),20,292,665,&tv4_ru_12,0x9fb4c8);
}
void firmware_view_update(firmware_view_t *view,const firmware_info_t *info) {
    if(!view->panel)return;
    static const string_id profiles[]={S_FW_UNKNOWN,S_FW_HEADLESS,S_FW_REV12,S_FW_MEMORY_BENCH,S_FW_TLS_BENCH};
    string_id source=info->source==FIRMWARE_INFO_DEVICE?S_FW_DEVICE:
        info->source==FIRMWARE_INFO_SIMULATOR?S_FW_SIMULATOR:S_FW_UNKNOWN;
    bool known=info->source!=FIRMWARE_INFO_UNKNOWN;
    lv_label_set_text(view->source,tr(NULL,source));
    lv_obj_set_style_text_color(view->source,lv_color_hex(info->source==FIRMWARE_INFO_DEVICE?0x9fb4c8:0xffcb61),0);
    lv_label_set_text(view->value[0],known?info->build:tr(NULL,S_FW_UNKNOWN));
    lv_label_set_text(view->value[1],known?info->sdk:tr(NULL,S_FW_UNKNOWN));
    unsigned profile=(unsigned)info->profile;
    lv_label_set_text(view->value[2],tr(NULL,profile<=FIRMWARE_PROFILE_TLS_BENCH?profiles[profile]:S_FW_UNKNOWN));
    lv_label_set_text(view->value[3],tr(NULL,S_FW_PEER_UNKNOWN));
    lv_label_set_text(view->value[4],tr(NULL,S_FW_PEER_UNKNOWN));
}
void firmware_view_destroy(firmware_view_t *view) {
    if(view->panel)lv_obj_delete(view->panel);
    memset(view,0,sizeof(*view));
}
