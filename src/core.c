/* SPDX-License-Identifier: GPL-3.0-only */
#include "mk360.h"

void mk_zero(void *p,size_t n) { uint8_t *d=p; while(n--) *d++=0; }
void mk_copy(void *dst,const void *src,size_t n) {
    uint8_t *d=dst; const uint8_t *s=src; while(n--) *d++=*s++;
}
bool mk_equal(const void *a,const void *b,size_t n) {
    const uint8_t *x=a,*y=b; while(n--) if(*x++!=*y++) return false; return true;
}
size_t mk_length(const char *s) { size_t n=0; while(s[n]) ++n; return n; }
static char upper(char c) { return c>='a'&&c<='z'?(char)(c-'a'+'A'):c; }
bool mk_text_equal(const char *a,const char *b) {
    while(*a&&*b) if(upper(*a++)!=upper(*b++)) return false;
    return *a==*b;
}
static int clamp(int x,int lo,int hi) { return x<lo?lo:(x>hi?hi:x); }
int mk_key_code(const char *name) {
    struct Name {const char *name;int code;};
    static const struct Name names[]={
        {"ENTER",40},{"ESC",41},{"BACKSPACE",42},{"TAB",43},{"SPACE",44},
        {"MINUS",45},{"EQUALS",46},{"CAPSLOCK",57},{"RIGHT",79},{"LEFT",80},
        {"DOWN",81},{"UP",82},{"LCTRL",224},{"LSHIFT",225},{"LALT",226},
        {"LGUI",227},{"RCTRL",228},{"RSHIFT",229},{"RALT",230},{"RGUI",231}
    };
    if(name[0]&&!name[1]) {
        char c=upper(name[0]);
        if(c>='A'&&c<='Z') return 4+c-'A';
        if(c>='1'&&c<='9') return 30+c-'1';
        if(c=='0') return 39;
    }
    for(size_t i=0;i<sizeof(names)/sizeof(names[0]);++i)
        if(mk_text_equal(name,names[i].name)) return names[i].code;
    return -1;
}
uint32_t mk_action(const char *name) {
    struct Action {const char *name;uint32_t action;};
    static const struct Action names[]={
        {"A",MK_A},{"B",MK_B},{"X",MK_X},{"Y",MK_Y},{"LB",MK_LB},{"RB",MK_RB},
        {"LT",MK_LT},{"RT",MK_RT},{"LS",MK_LS},{"RS",MK_RS},{"START",MK_START},
        {"BACK",MK_BACK},{"UP",MK_UP},{"DOWN",MK_DOWN},{"LEFT",MK_LEFT},{"RIGHT",MK_RIGHT},
        {"MOVE_UP",MK_MOVE_UP},{"MOVE_DOWN",MK_MOVE_DOWN},
        {"MOVE_LEFT",MK_MOVE_LEFT},{"MOVE_RIGHT",MK_MOVE_RIGHT},{"NONE",0}
    };
    for(size_t i=0;i<sizeof(names)/sizeof(names[0]);++i)
        if(mk_text_equal(name,names[i].name)) return names[i].action;
    return UINT32_MAX;
}
static void bind(MkConfig *c,const char *key,uint32_t action) { c->key[mk_key_code(key)]=action; }
void mk_defaults(MkConfig *c) {
    mk_zero(c,sizeof(*c));
    c->sensitivity=650; c->ads_percent=60; c->deadzone_x=7500; c->deadzone_y=7500;
    c->smoothing=25; c->enabled=1; c->require_xbdm=1;
    bind(c,"W",MK_MOVE_UP);bind(c,"S",MK_MOVE_DOWN);bind(c,"A",MK_MOVE_LEFT);bind(c,"D",MK_MOVE_RIGHT);
    bind(c,"SPACE",MK_A);bind(c,"C",MK_B);bind(c,"LCTRL",MK_B);bind(c,"R",MK_X);
    bind(c,"F",MK_X);bind(c,"1",MK_Y);bind(c,"Q",MK_LB);bind(c,"E",MK_RB);
    bind(c,"LSHIFT",MK_LS);bind(c,"V",MK_RS);bind(c,"ENTER",MK_START);bind(c,"ESC",MK_START);
    bind(c,"TAB",MK_BACK);bind(c,"UP",MK_UP);bind(c,"DOWN",MK_DOWN);
    bind(c,"LEFT",MK_LEFT);bind(c,"RIGHT",MK_RIGHT);
    c->mouse[0]=MK_RT;c->mouse[1]=MK_LT;c->mouse[2]=MK_RS;
}
void mk_input_init(MkInput *i,bool enabled) { mk_zero(i,sizeof(*i)); i->enabled=enabled; }
void mk_release_keyboard(MkInput *i) {
    mk_zero(i->keys,sizeof(i->keys));
    i->toggle_latch=i->down_latch=i->up_latch=i->reload_latch=false;
}
void mk_reset_motion(MkInput *i) { i->pending_x=i->pending_y=i->filtered_x=i->filtered_y=0; }
void mk_release_mouse(MkInput *i) { i->mouse_buttons=0;mk_reset_motion(i); }
bool mk_keyboard_report(MkInput *i,const uint8_t *r,size_t n) {
    if(!r||n<8) return false;
    for(unsigned k=2;k<8;k++) if(r[k]>0&&r[k]<4) return false; /* HID rollover/error report */
    mk_zero(i->keys,sizeof(i->keys));
    for(unsigned b=0;b<8;b++) if(r[0]&(1u<<b)) i->keys[224+b]=1;
    for(unsigned k=2;k<8;k++) if(r[k]>=4) i->keys[r[k]]=1;
    return true;
}
bool mk_mouse_report(MkInput *i,const uint8_t *r,size_t n,uint32_t now) {
    if(!r||n<3) return false;
    i->mouse_buttons=r[0]&7;
    i->pending_x=clamp(i->pending_x+(int8_t)r[1],-16384,16384);
    i->pending_y=clamp(i->pending_y+(int8_t)r[2],-16384,16384);
    i->mouse_at=now;
    return true;
}
static int map_axis(int delta,int sensitivity,int deadzone,uint32_t dt) {
    if(!delta) return 0;
    int magnitude=delta<0?-delta:delta;
    /* Divide first so the 32-bit product remains bounded even after a USB burst. */
    int speed=clamp(magnitude*4/(int)dt,0,16384);
    int value=deadzone+speed*sensitivity;
    value=clamp(value,0,32767);
    return delta<0?-value:value;
}
static bool edge(bool pressed,bool *latch) { bool rose=pressed&&!*latch;*latch=pressed;return rose; }
void mk_step(MkInput *i,const MkConfig *c,uint32_t now,uint32_t dt,MkPad *out) {
    mk_zero(out,sizeof(*out));
    if(edge(i->keys[63]!=0,&i->toggle_latch)) {i->enabled=!i->enabled;mk_reset_motion(i);} /* F6 */
    if(edge(i->keys[64]!=0,&i->down_latch)) i->sensitivity_delta=clamp(i->sensitivity_delta-50,-c->sensitivity+1,4000-c->sensitivity);
    if(edge(i->keys[65]!=0,&i->up_latch)) i->sensitivity_delta=clamp(i->sensitivity_delta+50,-c->sensitivity+1,4000-c->sensitivity);
    if(edge(i->keys[66]!=0,&i->reload_latch)) i->reload_requested=true; /* F9 */
    if(!i->enabled) {mk_reset_motion(i);return;}
    uint32_t actions=0;
    for(unsigned k=0;k<256;k++) if(i->keys[k]) actions|=c->key[k];
    for(unsigned k=0;k<3;k++) if(i->mouse_buttons&(1u<<k)) actions|=c->mouse[k];
    out->buttons=(uint16_t)actions;
    out->lt=(actions&MK_LT)?255:0;out->rt=(actions&MK_RT)?255:0;
    int x=((actions&MK_MOVE_RIGHT)?1:0)-((actions&MK_MOVE_LEFT)?1:0);
    int y=((actions&MK_MOVE_UP)?1:0)-((actions&MK_MOVE_DOWN)?1:0);
    int strength=x&&y?23170:32767; /* normalize diagonal movement */
    out->lx=(int16_t)(x*strength);out->ly=(int16_t)(y*strength);
    dt=dt<1?1:(dt>32?32:dt);
    int sensitivity=clamp(c->sensitivity+i->sensitivity_delta,1,4000);
    if(out->lt) sensitivity=sensitivity*c->ads_percent/100;
    int rx=map_axis(i->pending_x,sensitivity,c->deadzone_x,dt);
    int ry=map_axis(i->pending_y,sensitivity,c->deadzone_y,dt);
    if(!c->invert_y) ry=-ry;
    i->pending_x=i->pending_y=0;
    i->filtered_x=(i->filtered_x*c->smoothing+rx*(100-c->smoothing))/100;
    i->filtered_y=(i->filtered_y*c->smoothing+ry*(100-c->smoothing))/100;
    if((uint32_t)(now-i->mouse_at)>32) i->filtered_x=i->filtered_y=0;
    out->rx=(int16_t)i->filtered_x;out->ry=(int16_t)i->filtered_y;
}
