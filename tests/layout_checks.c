#include "layout_checks.h"
#include <assert.h>
#include <stdio.h>
static struct { lv_obj_t *obj; lv_area_t ink; const char *text; } labels[128];
static lv_obj_t *buttons[48];
static unsigned label_count, button_count;
static bool ancestor(lv_obj_t *parent,lv_obj_t *obj) {
    while((obj=lv_obj_get_parent(obj))) if(obj==parent) return true;
    return false;
}
static bool intersects(lv_area_t a,lv_area_t b) {
    return a.x1<=b.x2 && b.x1<=a.x2 && a.y1<=b.y2 && b.y1<=a.y2;
}
static void check_glyphs(const char *text,const lv_font_t *font) {
    const unsigned char *p=(const unsigned char *)text;
    while(*p) {
        uint32_t cp=*p++;unsigned continuation=0;
        if(cp>=0xf0) { cp&=7;continuation=3; }
        else if(cp>=0xe0) { cp&=15;continuation=2; }
        else if(cp>=0xc0) { cp&=31;continuation=1; }
        while(continuation--) { assert((*p&0xc0)==0x80);cp=(cp<<6)|(*p++&63); }
        if(cp=='\n' || cp=='\r' || cp=='\t')continue;
        lv_font_glyph_dsc_t glyph;
        bool found=lv_font_get_glyph_dsc(font,&glyph,cp,0);
        if(!found || glyph.is_placeholder)fprintf(stderr,"Missing glyph U+%04lx in: %s\n",(unsigned long)cp,text);
        assert(found && !glyph.is_placeholder);
    }
}
/* Compare visible ink inside scroll containers, without transform rounding. */
static bool clip_to_parents(lv_obj_t *obj,lv_area_t *area) {
    for(lv_obj_t *p=lv_obj_get_parent(obj);p;p=lv_obj_get_parent(p)) {
        lv_area_t clip;lv_obj_get_coords(p,&clip);
        if(area->x1<clip.x1)area->x1=clip.x1;
        if(area->y1<clip.y1)area->y1=clip.y1;
        if(area->x2>clip.x2)area->x2=clip.x2;
        if(area->y2>clip.y2)area->y2=clip.y2;
    }
    return area->x1<=area->x2 && area->y1<=area->y2;
}
static void collect(lv_obj_t *obj) {
    if(!lv_obj_is_visible(obj)) return;
    if(lv_obj_check_type(obj,&lv_label_class)) {
        const char *text=lv_label_get_text(obj);
        if(*text) {
            check_glyphs(text,lv_obj_get_style_text_font(obj,0));
            lv_point_t size;
            lv_text_get_size(&size,text,lv_obj_get_style_text_font(obj,0),
                lv_obj_get_style_text_letter_space(obj,0),lv_obj_get_style_text_line_space(obj,0),
                LV_COORD_MAX,LV_TEXT_FLAG_NONE);
            lv_area_t box,parent; lv_obj_get_coords(obj,&box);
            if(lv_label_get_long_mode(obj)==LV_LABEL_LONG_DOT && size.x>lv_obj_get_width(obj))size.x=lv_obj_get_width(obj);
            if(size.x>lv_obj_get_width(obj)) fprintf(stderr,"Wrapped/clipped label: %s\n",text);
            assert(size.x<=lv_obj_get_width(obj));
            lv_text_align_t align=lv_obj_get_style_text_align(obj,0);
            if(align==LV_TEXT_ALIGN_CENTER) box.x1+=(lv_obj_get_width(obj)-size.x)/2;
            if(align==LV_TEXT_ALIGN_RIGHT) box.x1+=lv_obj_get_width(obj)-size.x;
            box.x2=box.x1+size.x-1; box.y2=box.y1+size.y-1;
            lv_obj_get_coords(lv_obj_get_parent(obj),&parent);
            if(box.x1<parent.x1 || box.y1<parent.y1 || box.x2>parent.x2 || box.y2>parent.y2)
                fprintf(stderr,"Label outside parent: %s\n",text);
            assert(box.x1>=parent.x1 && box.y1>=parent.y1 && box.x2<=parent.x2 && box.y2<=parent.y2);
            assert(label_count<128);
            if(!clip_to_parents(obj,&box))return;
            labels[label_count].obj=obj; labels[label_count].ink=box;
            labels[label_count++].text=text;
        }
    }
    if(lv_obj_check_type(obj,&lv_button_class)) {
        assert(button_count<48); buttons[button_count++]=obj;
    }
    for(unsigned i=0;i<lv_obj_get_child_count(obj);i++) collect(lv_obj_get_child(obj,i));
}
void check_dashboard_layout(lv_obj_t *root) {
    lv_obj_update_layout(root); label_count=button_count=0; collect(root);
    for(unsigned i=0;i<button_count;i++) for(unsigned j=i+1;j<button_count;j++) {
        if(ancestor(buttons[i],buttons[j]) || ancestor(buttons[j],buttons[i])) continue;
        lv_area_t a,b; lv_obj_get_coords(buttons[i],&a); lv_obj_get_coords(buttons[j],&b);
        if(!clip_to_parents(buttons[i],&a) || !clip_to_parents(buttons[j],&b))continue;
        if(intersects(a,b)) { fputs("Overlapping buttons\n",stderr); assert(false); }
    }
    for(unsigned i=0;i<label_count;i++) {
        for(unsigned j=i+1;j<label_count;j++) {
            if(intersects(labels[i].ink,labels[j].ink)) {
                fprintf(stderr,"Overlapping text: %s / %s\n",labels[i].text,labels[j].text);
                assert(false);
            }
        }
        for(unsigned j=0;j<button_count;j++) {
            if(ancestor(buttons[j],labels[i].obj)) continue;
            lv_area_t box; lv_obj_get_coords(buttons[j],&box);
            if(!clip_to_parents(buttons[j],&box))continue;
            if(intersects(labels[i].ink,box)) {
                fprintf(stderr,"Button covers text: %s\n",labels[i].text); assert(false);
            }
        }
    }
}
