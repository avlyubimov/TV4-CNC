#include "tools.h"
#include <limits.h>
#include <string.h>
void tools_init(tools_t *t) {
    memset(t,0,sizeof(*t));
    for(unsigned i=0;i<TOOL_COUNT;i++)t->geometry[i].radius_um=i==2?0:400;
}
bool tools_geometry_valid(const tool_geometry_t *g) {
    /* Software schema limits only; not commissioned machine/tool limits. */
    return g && g->x_um>=-1000000 && g->x_um<=1000000 &&
        g->z_um>=-1000000 && g->z_um<=1000000 && g->radius_um>=0 && g->radius_um<=5000;
}
bool tools_save(tools_t *t,unsigned index,const tool_geometry_t *g) {
    if(!t || index>=TOOL_COUNT || !tools_geometry_valid(g))return false;
    t->geometry[index]=*g;t->revision++;return true;
}
bool tools_activate(tools_t *t,unsigned index) {
    if(!t || index>=TOOL_COUNT)return false;
    t->active=index;t->revision++;return true;
}
bool tools_parse_mm(const char *s,int32_t *out) {
    if(!s || !out)return false;
    unsigned length=0;while(length<16 && s[length])length++;
    if(!length || length==16)return false;
    unsigned i=0;bool neg=false;
    if(s[i]=='-' || s[i]=='+')neg=s[i++]=='-';
    unsigned digits=0,decimals=0;int64_t whole=0,fraction=0;
    while(s[i]>='0' && s[i]<='9') {
        whole=whole*10+s[i++]-'0';if(whole>2147483)return false;digits++;
    }
    if(!digits)return false;
    if(s[i]=='.' || s[i]==',') {
        i++;while(s[i]>='0' && s[i]<='9') {
            if(decimals==3)return false;
            fraction=fraction*10+s[i++]-'0';decimals++;
        }
        if(!decimals)return false;
    }
    if(s[i])return false;
    while(decimals++<3)fraction*=10;
    int64_t value=whole*1000+fraction;if(neg)value=-value;
    if(value<INT32_MIN || value>INT32_MAX)return false;
    *out=(int32_t)value;return true;
}
