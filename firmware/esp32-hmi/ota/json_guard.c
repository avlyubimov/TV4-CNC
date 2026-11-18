#include "json_guard.h"
#include <string.h>
/* This lexical preflight is a resource/encoding gate, NOT the JSON parser.
 * cJSON validates grammar. Reject embedded NUL, invalid UTF-8, escaped NUL,
 * fractional/exponent/negative numbers and >128 structural separators before
 * allocation. Signed schema uses unsigned integer fields only. */
static bool preflight(const uint8_t *s,size_t n) {
    unsigned separators=0;bool string=false,escape=false;
    for(size_t i=0;i<n;i++) {
        uint8_t c=s[i];if(!c)return false;
        if(c>=0x80) {
            unsigned extra;uint32_t cp,min;
            if(c>=0xc2 && c<=0xdf) {extra=1;cp=c&31u;min=0x80;}
            else if(c>=0xe0 && c<=0xef) {extra=2;cp=c&15u;min=0x800;}
            else if(c>=0xf0 && c<=0xf4) {extra=3;cp=c&7u;min=0x10000;}
            else return false;
            if(!string || escape || i+extra>=n)return false;
            while(extra--) {c=s[++i];if((c&0xc0)!=0x80)return false;cp=(cp<<6)|(c&63u);}
            if(cp<min || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))return false;
            continue;
        }
        if(string) {
            if(escape) {
                if(c=='u' && i+4<n && !memcmp(s+i+1,"0000",4))return false;
                escape=false;
            } else if(c=='\\')escape=true;
            else if(c=='"')string=false;
            else if(c<0x20)return false;
        } else if(c=='"')string=true;
        else if(c=='{' || c=='[' || c==',' || c==':') {if(++separators>128)return false;}
        else if((c>='0' && c<='9')) {
            size_t start=i;while(i+1<n && (s[i+1]>='0' && s[i+1]<='9'))++i;
            if(i>start && s[start]=='0')return false;
        } else if(c!='}' && c!=']' && c!=' ' && c!='\t' && c!='\r' && c!='\n')return false;
    }
    return !string && !escape;
}
static bool unique(const cJSON *node) {
    if(cJSON_IsObject(node)) {
        for(const cJSON *a=node->child;a;a=a->next) {
            if(!a->string)return false;
            for(const cJSON *b=a->next;b;b=b->next)if(!strcmp(a->string,b->string))return false;
        }
    }
    for(const cJSON *p=node->child;p;p=p->next)if(!unique(p))return false;
    return true;
}

cJSON *tv4_ota_json_parse(const uint8_t *raw,size_t length,size_t maximum) {
    if(!raw || !length || length>maximum || maximum>16384 || !preflight(raw,length))return NULL;
    const char *end=NULL;
    cJSON *root=cJSON_ParseWithLengthOpts((const char *)raw,length,&end,0);
    if(!root)return NULL;
    const char *limit=(const char *)raw+length;
    while(end<limit && (*end==' ' || *end=='\t' || *end=='\r' || *end=='\n'))++end;
    if(end!=limit || !unique(root)) {cJSON_Delete(root);return NULL;}
    return root;
}
