#include "boot_view.h"
LV_FONT_DECLARE(tv4_boot_56);
LV_FONT_DECLARE(tv4_ru_20);
LV_FONT_DECLARE(tv4_ru_14);
LV_FONT_DECLARE(tv4_ru_12);
static lv_obj_t *root,*card,*title,*stage_label,*progress,*percent,*detail,*dots[4],*captions[4];
static boot_stage_t current;
static bool failed;
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,int radius) {
    lv_obj_t *obj=lv_obj_create(parent);lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj,x,y);lv_obj_set_size(obj,w,h);lv_obj_set_style_radius(obj,radius,0);
    lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);lv_obj_set_style_bg_color(obj,lv_color_hex(color),0);
    lv_obj_remove_flag(obj,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);return obj;
}
static lv_obj_t *label(const char *text,int x,int y,int w,const lv_font_t *font,uint32_t color) {
    lv_obj_t *obj=lv_label_create(root);lv_label_set_text(obj,text);lv_obj_set_pos(obj,x,y);lv_obj_set_width(obj,w);
    lv_obj_set_style_text_font(obj,font,0);lv_obj_set_style_text_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_text_align(obj,LV_TEXT_ALIGN_CENTER,0);return obj;
}
static void update_steps(void) {
    for(unsigned i=0;i<4;i++) {
        bool done=current==BOOT_READY || i<(unsigned)current;
        bool active=current!=BOOT_READY && i==(unsigned)current;
        uint32_t color=active && failed?0xff4867:done?0x43dc98:active?0x19bfff:0x365064;
        lv_obj_set_style_bg_color(dots[i],lv_color_hex(color),0);
        lv_obj_set_style_shadow_color(dots[i],lv_color_hex(color),0);
        lv_obj_set_style_shadow_width(dots[i],active?12:0,0);
        lv_obj_set_style_shadow_opa(dots[i],LV_OPA_40,0);
        lv_obj_set_style_text_color(captions[i],lv_color_hex(done?0xb9dfd3:active?0xf0f8ff:0x6e879d),0);
    }
}
void boot_view_create(void) {
    root=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(root);
    lv_obj_set_size(root,800,480);lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root,LV_OBJ_FLAG_CLICKABLE); /* Absorb touches above the dashboard. */
    lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);lv_obj_set_style_bg_color(root,lv_color_hex(0x050b12),0);
    /* Flat low-contrast grid avoids coarse RGB565 gradient bands. */
    for(int x=32;x<800;x+=48)box(root,x,0,1,480,0x0a1420,0);
    for(int y=24;y<480;y+=48)box(root,0,y,800,1,0x0a1420,0);
    card=box(root,88,58,624,360,0x0c1824,20);
    lv_obj_set_style_border_width(card,1,0);lv_obj_set_style_border_color(card,lv_color_hex(0x2a435b),0);
    lv_obj_set_style_shadow_color(card,lv_color_hex(0x000000),0);lv_obj_set_style_shadow_width(card,40,0);
    lv_obj_set_style_shadow_opa(card,LV_OPA_70,0);lv_obj_set_style_shadow_offset_y(card,10,0);
    lv_obj_t *accent=box(root,160,58,480,2,0x0089d9,1);
    lv_obj_set_style_shadow_color(accent,lv_color_hex(0x008fff),0);lv_obj_set_style_shadow_width(accent,16,0);
    lv_obj_set_style_shadow_opa(accent,LV_OPA_30,0);
    label("TV4",238,104,142,&tv4_boot_56,0xf5f9ff);
    label("CNC",388,104,174,&tv4_boot_56,0x21adff);
    label(tr(NULL,S_LATHE),144,185,512,&tv4_ru_14,0x88a9c4);
    title=label(tr(NULL,S_BOOT_TITLE),144,232,512,&tv4_ru_20,0xf0f7ff);
    stage_label=label("",144,276,436,&tv4_ru_12,0xa6c3da);
    lv_obj_set_style_text_align(stage_label,LV_TEXT_ALIGN_LEFT,0);
    percent=label("",600,274,56,&tv4_ru_14,0x52caff);lv_obj_set_style_text_align(percent,LV_TEXT_ALIGN_RIGHT,0);
    progress=lv_bar_create(root);lv_obj_remove_style_all(progress);lv_obj_set_pos(progress,144,307);lv_obj_set_size(progress,512,7);
    lv_bar_set_range(progress,0,100);lv_obj_set_style_radius(progress,4,LV_PART_MAIN);
    lv_obj_set_style_bg_opa(progress,LV_OPA_COVER,LV_PART_MAIN);lv_obj_set_style_bg_color(progress,lv_color_hex(0x203449),LV_PART_MAIN);
    lv_obj_set_style_radius(progress,4,LV_PART_INDICATOR);lv_obj_set_style_bg_opa(progress,LV_OPA_COVER,LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(progress,lv_color_hex(0x12c8fa),LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(progress,lv_color_hex(0x006eff),LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(progress,LV_GRAD_DIR_HOR,LV_PART_INDICATOR);
    lv_obj_set_style_shadow_color(progress,lv_color_hex(0x00aaff),LV_PART_INDICATOR);
    lv_obj_set_style_shadow_width(progress,12,LV_PART_INDICATOR);lv_obj_set_style_shadow_opa(progress,LV_OPA_30,LV_PART_INDICATOR);
    static const string_id steps[]={S_BOOT_STEP_DISPLAY,S_BOOT_STEP_TOUCH,S_BOOT_STEP_NETWORK,S_BOOT_STEP_UI};
    for(unsigned i=0;i<4;i++) {
        dots[i]=box(root,204+(int)i*128,338,8,8,0x365064,4);
        captions[i]=label(tr(NULL,steps[i]),144+(int)i*128,357,128,&tv4_ru_12,0x6e879d);
    }
    detail=label("",128,391,544,&tv4_ru_12,0xff9eac);
    label(tr(NULL,S_BOOT_NOTE),64,445,672,&tv4_ru_12,0xc3a66c);
    current=BOOT_DISPLAY;failed=false;boot_view_stage(BOOT_DISPLAY);
}
void boot_view_stage(boot_stage_t stage) {
    if(!root || failed || stage<current || stage>=BOOT_STAGE_COUNT)return;
    static const string_id text[]={S_BOOT_DISPLAY,S_BOOT_TOUCH,S_BOOT_NETWORK,S_BOOT_INTERFACE,S_BOOT_READY};
    static const unsigned values[]={20,40,60,80,100};
    current=stage;lv_label_set_text(stage_label,tr(NULL,text[stage]));
    lv_bar_set_value(progress,(int)values[stage],LV_ANIM_OFF);lv_label_set_text_fmt(percent,"%u%%",values[stage]);
    update_steps();lv_obj_move_foreground(root);
}
void boot_view_fail(string_id reason,const char *code) {
    if(!root || failed)return;
    failed=true;lv_label_set_text(title,tr(NULL,S_BOOT_FAILED));lv_label_set_text(stage_label,tr(NULL,reason));
    lv_label_set_text(percent,"!");lv_label_set_text_fmt(detail,tr(NULL,S_BOOT_ERROR_CODE),code?code:"");
    lv_obj_set_style_text_color(stage_label,lv_color_hex(0xff8598),0);
    lv_obj_set_style_text_color(percent,lv_color_hex(0xff4867),0);
    lv_obj_set_style_border_color(card,lv_color_hex(0x723445),0);
    lv_obj_set_style_bg_color(progress,lv_color_hex(0xff2548),LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(progress,lv_color_hex(0xcb203e),LV_PART_INDICATOR);
    lv_obj_set_style_shadow_color(progress,lv_color_hex(0xff2548),LV_PART_INDICATOR);
    update_steps();lv_obj_move_foreground(root);
}
bool boot_view_finish(void) {
    if(!root || failed || current!=BOOT_READY)return false;
    lv_obj_delete(root);root=NULL;card=title=stage_label=progress=percent=detail=NULL;
    for(unsigned i=0;i<4;i++)dots[i]=captions[i]=NULL;
    return true;
}
