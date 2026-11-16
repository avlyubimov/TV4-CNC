#include "tools_view.h"
#include "strings.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
LV_FONT_DECLARE(tv4_ru_12);
LV_FONT_DECLARE(tv4_ru_14);
LV_FONT_DECLARE(tv4_ru_20);
LV_FONT_DECLARE(tv4_button_12);
enum { A_ROW=1,A_MINUS=10,A_PLUS=20,A_FIELD=30,A_SAVE=40,A_CANCEL,A_ACTIVATE,
       A_DIGIT=50,A_SIGN=60,A_COMMA,A_BACKSPACE,A_CLEAR,A_CLOSE,A_ACCEPT };
static unsigned intents[8],read_index,write_index,used,dropped,selected,field;
static bool initialized,modal,replace_input;
static tool_geometry_t draft,baseline;
static uint32_t revision;
static char input[16];
static string_id message;
static lv_obj_t *root,*content,*popup,*rows[TOOL_COUNT],*values[3],*heading,*active,*feedback,*input_label,*input_feedback,*input_title;
static const string_id short_names[]={S_TOOL_SHORT_1,S_TOOL_SHORT_2,S_TOOL_SHORT_3,S_TOOL_SHORT_4};
static const string_id field_names[]={S_TOOL_X,S_TOOL_Z,S_TOOL_R};
static lv_obj_t *text(lv_obj_t *parent,const char *value,int x,int y,int width) {
    lv_obj_t *o=lv_label_create(parent);lv_label_set_text(o,value);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,width);return o;
}
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h) {
    lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_bg_color(o,lv_color_hex(0x172631),0);
    lv_obj_set_style_bg_grad_color(o,lv_color_hex(0x080f15),0);lv_obj_set_style_bg_grad_dir(o,LV_GRAD_DIR_VER,0);
    lv_obj_set_style_radius(o,8,0);lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,lv_color_hex(0x344752),0);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);return o;
}
static void clicked(lv_event_t *event) {
    if(used==8) { dropped++;return; }
    intents[write_index]=(unsigned)(uintptr_t)lv_event_get_user_data(event);write_index=(write_index+1)%8;used++;
}
static lv_obj_t *control(lv_obj_t *parent,const char *caption,unsigned action,int x,int y,int w,int h) {
    lv_obj_t *b=lv_button_create(parent);lv_obj_remove_style_all(b);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,h);
    lv_obj_set_style_bg_opa(b,LV_OPA_COVER,0);lv_obj_set_style_bg_color(b,lv_color_hex(0x354451),0);
    lv_obj_set_style_bg_grad_color(b,lv_color_hex(0x111c25),0);lv_obj_set_style_bg_grad_dir(b,LV_GRAD_DIR_VER,0);
    lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_border_color(b,lv_color_hex(0x657d91),0);
    lv_obj_set_style_radius(b,7,0);lv_obj_set_style_text_font(b,&tv4_button_12,0);
    lv_obj_set_style_bg_color(b,lv_color_hex(0x058be4),LV_STATE_PRESSED);
    lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,caption);lv_obj_center(l);
    lv_obj_add_event_cb(b,clicked,LV_EVENT_CLICKED,(void *)(uintptr_t)action);return b;
}
static int32_t *component(tool_geometry_t *g,unsigned index) {
    return index==0?&g->x_um:index==1?&g->z_um:&g->radius_um;
}
static bool same(tool_geometry_t a,tool_geometry_t b) {
    return a.x_um==b.x_um && a.z_um==b.z_um && a.radius_um==b.radius_um;
}
static bool dirty(const model_t *m) { (void)m;return !same(draft,baseline); }
static void format_mm(char *buffer,size_t size,int32_t um) {
    long v=um;snprintf(buffer,size,"%s%ld,%03ld",v<0?"-":"",(v<0?-v:v)/1000,(v<0?-v:v)%1000);
}
static void paint(const model_t *m) {
    for(unsigned i=0;i<TOOL_COUNT;i++) {
        lv_obj_set_style_bg_color(rows[i],lv_color_hex(i==selected?0x008ce8:0x354451),0);
        lv_obj_set_style_border_color(rows[i],lv_color_hex(i==selected?0x40d9ff:0x657d91),0);
    }
    lv_label_set_text_fmt(heading,tr(0,S_FMT_TOOL_ROW),selected+1,tr(0,short_names[selected]));
    lv_label_set_text_fmt(active,tr(0,S_FMT_ACTIVE_TOOL),m->tools.active+1);
    lv_label_set_text(feedback,tr(0,message));
    for(unsigned i=0;i<3;i++) {
        char value[24];format_mm(value,sizeof(value),*component(&draft,i));
        lv_label_set_text(lv_obj_get_child(values[i],0),value);
    }
    if(modal) {
        lv_obj_add_flag(content,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(popup,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(input_title,tr(0,field_names[field]));
        lv_label_set_text(input_label,input);
    } else { lv_obj_remove_flag(content,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(popup,LV_OBJ_FLAG_HIDDEN); }
}
static bool execute(model_t *m,unsigned action) {
    if(action>=A_ROW && action<A_ROW+TOOL_COUNT) {
        if(dirty(m))message=S_TOOL_UNSAVED;
        else { selected=action-A_ROW;draft=baseline=m->tools.geometry[selected];message=S_TOOL_SELECT_HINT; }
    } else if(action>=A_MINUS && action<A_MINUS+3) {
        tool_geometry_t next=draft;unsigned f=action-A_MINUS;*component(&next,f)-=f==2?50:100;
        if(tools_geometry_valid(&next)) { draft=next;message=dirty(m)?S_TOOL_DRAFT:S_TOOL_STORED; } else message=S_TOOL_RANGE;
    } else if(action>=A_PLUS && action<A_PLUS+3) {
        tool_geometry_t next=draft;unsigned f=action-A_PLUS;*component(&next,f)+=f==2?50:100;
        if(tools_geometry_valid(&next)) { draft=next;message=dirty(m)?S_TOOL_DRAFT:S_TOOL_STORED; } else message=S_TOOL_RANGE;
    } else if(action>=A_FIELD && action<A_FIELD+3) {
        field=action-A_FIELD;format_mm(input,sizeof(input),*component(&draft,field));
        modal=true;replace_input=true;lv_label_set_text(input_feedback,"");
    } else if(action==A_SAVE) {
        if(!same(baseline,m->tools.geometry[selected]))message=S_TOOL_CONFLICT;
        else if(model_save_tool(m,selected,&draft)) { baseline=draft;revision=m->tools.revision;message=S_TOOL_SAVED; }
        else message=S_TOOL_LOCKED;
    } else if(action==A_CANCEL) { draft=baseline=m->tools.geometry[selected];message=S_TOOL_STORED;
    } else if(action==A_ACTIVATE) {
        if(dirty(m))message=S_TOOL_UNSAVED;
        else if(model_activate_tool(m,selected)) { revision=m->tools.revision;message=S_TOOL_ACTIVATED; }
        else message=S_TOOL_LOCKED;
    } else if(modal) {
        size_t n=strlen(input);
        if(action==A_CLOSE) { modal=false;return true; }
        if(action==A_ACCEPT) {
            int32_t value;tool_geometry_t next=draft;
            if(!tools_parse_mm(input,&value)) { lv_label_set_text(input_feedback,tr(0,S_TOOL_INPUT_ERROR));return true; }
            *component(&next,field)=value;
            if(!tools_geometry_valid(&next)) { lv_label_set_text(input_feedback,tr(0,S_TOOL_RANGE));return true; }
            draft=next;modal=false;message=dirty(m)?S_TOOL_DRAFT:S_TOOL_STORED;return true;
        }
        if(action==A_CLEAR || (action==A_BACKSPACE && replace_input))input[0]=0;
        else if(action==A_BACKSPACE) { if(n)input[n-1]=0;
        } else if(action==A_SIGN) {
            if(replace_input) { input[0]='-';input[1]=0; }
            else if(input[0]=='-')memmove(input,input+1,n);
            else if(n+1<sizeof(input)) { memmove(input+1,input,n+1);input[0]='-'; }
        } else if((action>=A_DIGIT && action<A_DIGIT+10) || action==A_COMMA) {
            if(replace_input) { input[0]=0;n=0; }
            if(n+1<sizeof(input)) { input[n]=action==A_COMMA?',':(char)('0'+action-A_DIGIT);input[n+1]=0; }
        }
        replace_input=false;lv_label_set_text(input_feedback,"");
    }
    return true;
}
lv_obj_t *tools_view_create(lv_obj_t *parent) {
    initialized=modal=false;used=read_index=write_index=dropped=selected=0;revision=0;message=S_TOOL_SELECT_HINT;
    root=box(parent,89,56,705,347);lv_obj_set_style_text_font(root,&tv4_ru_12,0);
    content=lv_obj_create(root);lv_obj_remove_style_all(content);lv_obj_set_size(content,705,347);lv_obj_remove_flag(content,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *title=text(content,tr(0,S_TOOLS),14,10,674);lv_obj_set_style_text_font(title,&tv4_ru_20,0);
    text(content,tr(0,S_TOOL_RAM),14,40,300);text(content,tr(0,S_TOOL_NOTE),330,40,360);
    for(unsigned i=0;i<TOOL_COUNT;i++) {
        char caption[80];snprintf(caption,sizeof(caption),tr(0,S_FMT_TOOL_ROW),i+1,tr(0,short_names[i]));
        rows[i]=control(content,caption,A_ROW+i,14,70+i*54,186,44);
    }
    active=text(content,"",14,292,186);text(content,tr(0,S_TOOL_STORED),14,316,186);
    lv_obj_t *editor=box(content,214,68,477,210);
    heading=text(editor,"",14,12,280);lv_obj_set_style_text_font(heading,&tv4_ru_20,0);
    control(editor,tr(0,S_ACTIVATE),A_ACTIVATE,304,8,159,36);
    for(unsigned i=0;i<3;i++) {
        text(editor,tr(0,field_names[i]),14,64+i*52,132);
        values[i]=control(editor,"",A_FIELD+i,156,52+i*52,128,40);
        lv_obj_set_style_text_font(values[i],&tv4_ru_20,0);
        control(editor,tr(0,i==2?S_TOOL_DEC_R:S_TOOL_DEC_XZ),A_MINUS+i,300,52+i*52,64,40);
        control(editor,tr(0,i==2?S_TOOL_INC_R:S_TOOL_INC_XZ),A_PLUS+i,380,52+i*52,64,40);
    }
    control(content,tr(0,S_SAVE),A_SAVE,228,286,145,36);
    control(content,tr(0,S_CANCEL),A_CANCEL,385,286,145,36);
    feedback=text(content,"",228,328,460);
    popup=box(root,137,14,430,319);
    input_title=text(popup,"",16,10,398);
    input_label=text(popup,"",16,38,398);lv_obj_set_style_text_font(input_label,&tv4_ru_20,0);
    for(unsigned i=1;i<=9;i++) {
        char digit[2]={(char)('0'+i),0};control(popup,digit,A_DIGIT+i,16+((i-1)%3)*72,88+((i-1)/3)*52,64,44);
    }
    control(popup,"±",A_SIGN,16,244,64,44);control(popup,"0",A_DIGIT,88,244,64,44);control(popup,",",A_COMMA,160,244,64,44);
    control(popup,tr(0,S_BACKSPACE),A_BACKSPACE,248,88,164,44);
    control(popup,tr(0,S_CLEAR),A_CLEAR,248,140,164,44);
    control(popup,tr(0,S_CANCEL),A_CLOSE,248,192,164,44);
    control(popup,tr(0,S_ACCEPT),A_ACCEPT,248,244,164,44);
    input_feedback=text(popup,"",16,294,398);
    lv_obj_add_flag(popup,LV_OBJ_FLAG_HIDDEN);return root;
}
void tools_view_update(model_t *m) {
    bool changed=false;
    if(!initialized) { selected=m->tools.active;draft=baseline=m->tools.geometry[selected];revision=m->tools.revision;initialized=true;changed=true; }
    if(dropped) { m->rejected+=dropped;dropped=0;model_event(m,EV_QUEUE_FAULT); }
    while(used) { unsigned action=intents[read_index];read_index=(read_index+1)%8;used--;changed|=execute(m,action); }
    if(revision!=m->tools.revision) { revision=m->tools.revision;if(!dirty(m))draft=baseline=m->tools.geometry[selected];changed=true; }
    if(changed)paint(m);
}
