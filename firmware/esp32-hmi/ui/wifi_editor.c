#include "wifi_editor.h"
#include "strings.h"
#include <string.h>
LV_FONT_DECLARE(tv4_ru_20);
LV_FONT_DECLARE(tv4_ru_14);
static lv_obj_t *editor,*input,*target,*keyboard,*show_button;
static lv_font_t key_font;
static bool secret,close_requested,apply_requested;
static const char *const lower[]={
 "1","2","3","4","5","6","7","8","9","0",LV_SYMBOL_BACKSPACE,"\n",
 "q","w","e","r","t","y","u","i","o","p","\n",
 "a","s","d","f","g","h","j","k","l","-","\n",
 "ABC","z","x","c","v","b","n","m","_",".","\n",
 "1#",LV_SYMBOL_LEFT," ",LV_SYMBOL_RIGHT,"@",LV_SYMBOL_OK,""};
static const char *const upper[]={
 "1","2","3","4","5","6","7","8","9","0",LV_SYMBOL_BACKSPACE,"\n",
 "Q","W","E","R","T","Y","U","I","O","P","\n",
 "A","S","D","F","G","H","J","K","L","-","\n",
 "abc","Z","X","C","V","B","N","M","_",".","\n",
 "1#",LV_SYMBOL_LEFT," ",LV_SYMBOL_RIGHT,"@",LV_SYMBOL_OK,""};
static const char *const symbols[]={
 "1","2","3","4","5","6","7","8","9","0",LV_SYMBOL_BACKSPACE,"\n",
 "!","@","#","$","%","^","&","*","(",")","\n",
 "-","_","+","=","/","\\",":",";","\"","'","\n",
 "abc","[","]","{","}","<",">","?","~","`","\n",
 "ABC",LV_SYMBOL_LEFT," ",LV_SYMBOL_RIGHT,".",LV_SYMBOL_OK,""};
static lv_buttonmatrix_ctrl_t controls[3][64];
static void request(lv_event_t *e) { apply_requested=(uintptr_t)lv_event_get_user_data(e)!=0;close_requested=true; }
static void show_secret(lv_event_t *e) {
    (void)e;if(!secret)return;
    bool mask=!lv_textarea_get_password_mode(input);lv_textarea_set_password_mode(input,mask);
    lv_label_set_text(lv_obj_get_child(show_button,0),tr(NULL,mask?S_INPUT_SHOW:S_INPUT_HIDE));
}
static void key(lv_event_t *e) {
    lv_obj_t *kb=lv_event_get_target(e);
    const char *s=lv_keyboard_get_button_text(kb,lv_keyboard_get_selected_button(kb));if(!s)return;
    if(!strcmp(s,"ABC"))lv_keyboard_set_mode(kb,LV_KEYBOARD_MODE_USER_2);
    else if(!strcmp(s,"abc"))lv_keyboard_set_mode(kb,LV_KEYBOARD_MODE_USER_1);
    else if(!strcmp(s,"1#"))lv_keyboard_set_mode(kb,LV_KEYBOARD_MODE_USER_3);
    else if(!strcmp(s,LV_SYMBOL_OK)) { apply_requested=true;close_requested=true; }
    else if(!strcmp(s,LV_SYMBOL_BACKSPACE))lv_textarea_delete_char(input);
    else if(!strcmp(s,LV_SYMBOL_LEFT))lv_textarea_cursor_left(input);
    else if(!strcmp(s,LV_SYMBOL_RIGHT))lv_textarea_cursor_right(input);
    else lv_textarea_add_text(input,s);
}
static lv_obj_t *button(string_id id,int x,int y,int w,int h,lv_event_cb_t cb,uintptr_t data) {
    lv_obj_t *b=lv_button_create(editor);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,h);
    lv_obj_set_style_bg_color(b,lv_color_hex(0x24425b),0);
    lv_obj_set_style_text_font(b,&tv4_ru_14,0);
    lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,tr(NULL,id));lv_obj_center(l);
    lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,(void *)data);return b;
}
void wifi_editor_hide(void) {
    if(!editor)return;
    lv_keyboard_set_textarea(keyboard,NULL);lv_textarea_set_text(input,"");
    lv_obj_delete(editor);editor=input=target=keyboard=show_button=NULL;
    close_requested=apply_requested=false;
}
bool wifi_editor_is_open(void) { return editor!=NULL; }
void wifi_editor_open(lv_obj_t *field,bool is_secret) {
    wifi_editor_hide();target=field;secret=is_secret;
    editor=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(editor);lv_obj_set_size(editor,800,480);
    lv_obj_set_style_bg_opa(editor,LV_OPA_COVER,0);lv_obj_set_style_bg_color(editor,lv_color_hex(0x0b1723),0);
    lv_obj_remove_flag(editor,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(editor,LV_OBJ_FLAG_CLICKABLE);
    button(S_CANCEL,16,12,124,44,request,0);button(S_INPUT_DONE,660,12,124,44,request,1);
    lv_obj_t *title=lv_label_create(editor);lv_label_set_text(title,tr(NULL,secret?S_WIFI_PASSWORD:S_WIFI_SSID));
    lv_obj_set_pos(title,160,21);lv_obj_set_width(title,480);
    lv_obj_set_style_text_font(title,&tv4_ru_20,0);lv_obj_set_style_text_color(title,lv_color_hex(0xf2f8ff),0);
    lv_obj_set_style_text_align(title,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_t *hint=lv_label_create(editor);lv_label_set_text(hint,tr(NULL,S_INPUT_CASE));
    lv_obj_set_pos(hint,20,66);lv_obj_set_style_text_font(hint,&tv4_ru_14,0);
    lv_obj_set_style_text_color(hint,lv_color_hex(0xa3c6df),0);
    input=lv_textarea_create(editor);lv_obj_set_pos(input,20,92);lv_obj_set_size(input,secret?600:760,58);
    lv_obj_set_style_text_font(input,&tv4_ru_20,0);lv_obj_set_style_text_color(input,lv_color_hex(0xf5faff),0);
    lv_obj_set_style_bg_color(input,lv_color_hex(0x152a3b),0);
    lv_textarea_set_one_line(input,true);lv_textarea_set_max_length(input,secret?63:32);
    lv_textarea_set_password_mode(input,secret);lv_textarea_set_password_bullet(input,"*");lv_textarea_set_password_show_time(input,0);
    lv_textarea_set_text(input,lv_textarea_get_text(field));lv_textarea_set_cursor_pos(input,LV_TEXTAREA_CURSOR_LAST);
    show_button=button(S_INPUT_SHOW,636,92,144,58,show_secret,0);
    if(!secret)lv_obj_add_flag(show_button,LV_OBJ_FLAG_HIDDEN);
    keyboard=lv_keyboard_create(editor);lv_obj_set_align(keyboard,LV_ALIGN_TOP_LEFT);lv_obj_set_pos(keyboard,12,166);lv_obj_set_size(keyboard,776,300);
    lv_obj_remove_event_cb(keyboard,lv_keyboard_def_event_cb);
    const char *const *maps[]={lower,upper,symbols};
    for(unsigned m=0;m<3;m++) {
        unsigned n=0;for(unsigned i=0;maps[m][i][0];i++)if(strcmp(maps[m][i],"\n")) {
            controls[m][n++]=(lv_buttonmatrix_ctrl_t)((!strcmp(maps[m][i]," ")?4:!strcmp(maps[m][i],LV_SYMBOL_BACKSPACE)?2:1)|LV_BUTTONMATRIX_CTRL_CLICK_TRIG|LV_BUTTONMATRIX_CTRL_NO_REPEAT);
        }
        lv_keyboard_set_map(keyboard,LV_KEYBOARD_MODE_USER_1+m,maps[m],controls[m]);
    }
    lv_keyboard_set_mode(keyboard,LV_KEYBOARD_MODE_USER_1);lv_keyboard_set_textarea(keyboard,input);
    key_font=tv4_ru_20;key_font.fallback=LV_FONT_DEFAULT;
    lv_obj_set_style_text_font(keyboard,&key_font,LV_PART_ITEMS);lv_obj_set_style_text_color(keyboard,lv_color_hex(0xf4faff),LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keyboard,lv_color_hex(0x0b1723),0);lv_obj_set_style_pad_all(keyboard,4,0);
    lv_obj_set_style_pad_row(keyboard,6,0);lv_obj_set_style_pad_column(keyboard,6,0);
    lv_obj_set_style_bg_color(keyboard,lv_color_hex(0x29445c),LV_PART_ITEMS);lv_obj_set_style_bg_opa(keyboard,LV_OPA_COVER,LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keyboard,lv_color_hex(0x008fe8),LV_PART_ITEMS|LV_STATE_PRESSED);
    lv_obj_set_style_radius(keyboard,7,LV_PART_ITEMS);lv_obj_set_style_pad_all(keyboard,2,LV_PART_ITEMS);
    lv_obj_add_event_cb(keyboard,key,LV_EVENT_VALUE_CHANGED,NULL);
}
void wifi_editor_update(void) {
    if(!editor || !close_requested)return;
    if(apply_requested)lv_textarea_set_text(target,lv_textarea_get_text(input));
    wifi_editor_hide();
}
