/* SPDX-License-Identifier: GPL-3.0-only */
#include "mk360.h"

static char *trim(char *s) {
    while(*s==' '||*s=='\t'||*s=='\r') ++s;
    size_t n=mk_length(s);
    while(n&&(s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r')) s[--n]=0;
    return s;
}
static bool prefix(const char *s,const char *p) {
    while(*p) {char a=*s++,b=*p++;if(a>='a'&&a<='z')a-=32;if(b>='a'&&b<='z')b-=32;if(a!=b)return false;}
    return true;
}
static bool number(const char *s,int lo,int hi,int *out) {
    if(!*s)return false;
    unsigned value=0;
    while(*s) {
        if(*s<'0'||*s>'9'||value>100000u)return false;
        value=value*10+(unsigned)(*s++-'0');
    }
    if(value<(unsigned)lo||value>(unsigned)hi)return false;
    *out=(int)value;return true;
}
static bool section_id(const char *s,uint32_t *id) {
    if(mk_length(s)!=8)return false;
    uint32_t v=0;
    for(unsigned i=0;i<8;i++) {
        unsigned c=(unsigned char)s[i],digit;
        if(c>='0'&&c<='9')digit=c-'0';
        else if(c>='a'&&c<='f')digit=c-'a'+10;
        else if(c>='A'&&c<='F')digit=c-'A'+10;
        else return false;
        v=(v<<4)|digit;
    }
    *id=v;return true;
}
static bool setting(MkConfig *c,char *key,char *value) {
    struct Setting {const char *name;int *value;int lo,hi;};
    struct Setting fields[]={
        {"sensitivity",&c->sensitivity,1,4000},{"ads_percent",&c->ads_percent,1,200},
        {"deadzone_x",&c->deadzone_x,0,30000},{"deadzone_y",&c->deadzone_y,0,30000},
        {"smoothing",&c->smoothing,0,90},{"invert_y",&c->invert_y,0,1},
        {"enabled",&c->enabled,0,1},{"require_xbdm",&c->require_xbdm,0,1}
    };
    for(size_t n=0;n<sizeof(fields)/sizeof(fields[0]);n++)
        if(mk_text_equal(key,fields[n].name))return number(value,fields[n].lo,fields[n].hi,fields[n].value);
    if(prefix(key,"bind_")) {
        int code=mk_key_code(key+5);uint32_t action=mk_action(value);
        if(code<0||action==UINT32_MAX)return false;
        c->key[code]=action;return true;
    }
    if(prefix(key,"mouse")&&mk_length(key)==6&&key[5]>='1'&&key[5]<='3') {
        uint32_t action=mk_action(value);if(action==UINT32_MAX)return false;
        c->mouse[(unsigned)(key[5]-'1')]=action;return true;
    }
    return false;
}
bool mk_parse_config(MkConfig *out,const char *text,size_t length,uint32_t title,unsigned *bad_line) {
    if(bad_line)*bad_line=0;
    if(!text||length>MK360_CONFIG_LIMIT)return false;
    for(size_t n=0;n<length;n++)if(!text[n])return false;
    MkConfig next;mk_defaults(&next);
    /* Two passes make title overrides independent of section order in the file. */
    for(unsigned pass=0;pass<2;pass++) {
        size_t offset=0;unsigned line=0;bool selected=pass==0;
        if(length>=3&&(uint8_t)text[0]==0xef&&(uint8_t)text[1]==0xbb&&(uint8_t)text[2]==0xbf)offset=3;
        while(offset<length) {
            char buffer[256];size_t n=0;++line;
            while(offset<length&&text[offset]!='\n') {
                if(n==sizeof(buffer)-1)goto fail;
                buffer[n++]=text[offset++];
            }
            if(offset<length)++offset;
            buffer[n]=0;
            for(size_t k=0;k<n;k++)if(buffer[k]=='#'||buffer[k]==';'){buffer[k]=0;break;}
            char *s=trim(buffer);if(!*s)continue;
            if(*s=='[') {
                n=mk_length(s);if(n<3||s[n-1]!=']')goto fail;
                s[n-1]=0;s=trim(s+1);uint32_t id;
                if(mk_text_equal(s,"default"))selected=pass==0;
                else if(section_id(s,&id))selected=pass==1&&id==title;
                else goto fail;
                continue;
            }
            char *equal=s;while(*equal&&*equal!='=')++equal;
            if(!*equal)goto fail;
            *equal++=0;
            if(selected&&!setting(&next,trim(s),trim(equal)))goto fail;
            continue;
fail:
            if(bad_line)*bad_line=line;
            return false; /* caller's existing settings are untouched */
        }
    }
    *out=next;return true;
}
