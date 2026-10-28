/* SPDX-License-Identifier: GPL-3.0-only */
#include "mk360.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);}} while(0)
static void keys(MkInput *i,uint8_t modifiers,uint8_t a,uint8_t b,uint8_t c) {
    uint8_t r[8]={modifiers,0,a,b,c,0,0,0};CHECK(mk_keyboard_report(i,r,sizeof(r)));
}
static void movement_and_buttons(void) {
    MkConfig c;MkInput i;MkPad p;mk_defaults(&c);mk_input_init(&i,true);
    keys(&i,2,26,7,44); /* shift W D space */
    mk_step(&i,&c,4,4,&p);
    CHECK(p.lx==23170&&p.ly==23170);CHECK((p.buttons&(MK_A|MK_LS))==(MK_A|MK_LS));
    keys(&i,0,26,22,0);mk_step(&i,&c,8,4,&p);CHECK(p.ly==0&&p.lx==0);
    keys(&i,1,21,0,0);mk_step(&i,&c,12,4,&p);CHECK((p.buttons&(MK_B|MK_X))==(MK_B|MK_X));
    mk_release_keyboard(&i);mk_step(&i,&c,16,4,&p);CHECK(p.buttons==0&&p.lx==0&&p.ly==0);
}
static void aim_and_disconnect(void) {
    MkConfig c;MkInput i;MkPad p;mk_defaults(&c);c.smoothing=0;mk_input_init(&i,true);
    uint8_t mouse[]={1,10,251};CHECK(mk_mouse_report(&i,mouse,3,100));
    mk_step(&i,&c,100,4,&p);CHECK(p.rt==255&&p.lt==0&&p.rx>0&&p.ry>0);
    int hip=p.rx;mouse[0]=2;CHECK(mk_mouse_report(&i,mouse,3,104));mk_step(&i,&c,104,4,&p);
    CHECK(p.lt==255&&p.rt==0&&p.rx<hip);
    c.invert_y=1;CHECK(mk_mouse_report(&i,mouse,3,108));mk_step(&i,&c,108,4,&p);CHECK(p.ry<0);
    c.smoothing=90;mouse[1]=127;mouse[2]=127;
    for(unsigned n=0;n<10000;n++)CHECK(mk_mouse_report(&i,mouse,3,112));
    CHECK(i.pending_x==16384&&i.pending_y==16384);
    mk_step(&i,&c,112,4,&p);CHECK(p.rx>0&&p.rx<=32767);
    mk_step(&i,&c,145,4,&p);CHECK(p.rx==0&&p.ry==0); /* no stale aim */
    mk_release_mouse(&i);mk_step(&i,&c,149,4,&p);CHECK(p.lt==0&&p.rt==0&&p.rx==0&&p.ry==0);
    CHECK(!mk_mouse_report(&i,mouse,2,150));
    mouse[1]=1;mouse[2]=0;mk_mouse_report(&i,mouse,3,UINT32_MAX-4);
    mk_step(&i,&c,40,4,&p);CHECK(p.rx==0); /* monotonic timer wrap */
}
static void report_and_hotkeys(void) {
    MkInput i;MkConfig c;MkPad p;mk_defaults(&c);mk_input_init(&i,true);
    keys(&i,0,26,0,0);uint8_t rollover[8]={0,0,1,1,1,1,1,1};
    CHECK(!mk_keyboard_report(&i,rollover,8));CHECK(i.keys[26]);
    CHECK(!mk_keyboard_report(&i,rollover,7));CHECK(!mk_keyboard_report(&i,0,8));
    keys(&i,0,63,0,0);mk_step(&i,&c,4,4,&p);CHECK(!i.enabled);
    mk_step(&i,&c,8,4,&p);CHECK(!i.enabled); /* holding F6 must not repeat */
    keys(&i,0,0,0,0);mk_step(&i,&c,12,4,&p);
    keys(&i,0,63,26,0);mk_step(&i,&c,16,4,&p);CHECK(i.enabled&&p.ly>0);
    keys(&i,0,64,0,0);mk_step(&i,&c,20,4,&p);CHECK(i.sensitivity_delta==-50);
    keys(&i,0,65,66,0);mk_step(&i,&c,24,4,&p);CHECK(i.sensitivity_delta==0&&i.reload_requested);
}
static void configs(void) {
    const char text[]="\xEF\xBB\xBF[415608CB]\r\nsensitivity=1000\r\nbind_SPACE=B\n[default]\nsensitivity=600\nads_percent=50\nmouse1=LT\n";
    MkConfig c;mk_defaults(&c);unsigned line;
    CHECK(mk_parse_config(&c,text,sizeof(text)-1,0x415608CB,&line));
    CHECK(c.sensitivity==1000&&c.ads_percent==50&&c.key[44]==MK_B&&c.mouse[0]==MK_LT);
    CHECK(mk_parse_config(&c,text,sizeof(text)-1,0x12345678,&line));CHECK(c.sensitivity==600&&c.key[44]==MK_A);
    const char *bad[]={"sensitivity=-1","smoothing=100","bind_TYPO=A","mouse4=A","invert_y=2","sensitivity=999999999999","[not_a_title]","sensitivity=12\nunknown=1"};
    for(size_t n=0;n<sizeof(bad)/sizeof(bad[0]);n++) {
        MkConfig old=c;CHECK(!mk_parse_config(&c,bad[n],strlen(bad[n]),0,&line));CHECK(mk_equal(&old,&c,sizeof(c)));
    }
    char embedded[]={'e','n','a','b','l','e','d','=','0',0,'x'};
    CHECK(!mk_parse_config(&c,embedded,sizeof(embedded),0,&line));
    CHECK(mk_parse_config(&c,"",0,0,&line));CHECK(c.sensitivity==650);
    CHECK(mk_key_code("RCTRL")==228&&mk_key_code("w")==26&&mk_key_code("0")==39);
    CHECK(mk_action("RT")==MK_RT&&mk_action("invalid")==UINT32_MAX);
}
static void sampling(void) {
    MkConfig c;MkInput a,b;MkPad x,y;mk_defaults(&c);c.smoothing=0;c.deadzone_x=0;
    mk_input_init(&a,true);mk_input_init(&b,true);uint8_t r[]={0,8,0},s[]={0,16,0};
    mk_mouse_report(&a,r,3,8);mk_mouse_report(&b,s,3,8);
    mk_step(&a,&c,8,4,&x);mk_step(&b,&c,8,8,&y);CHECK(x.rx==y.rx);
}
static uint32_t random_state=0x360;
static uint32_t random32(void) {random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static void malformed_inputs(void) {
    MkConfig c;MkInput i;MkPad p;mk_defaults(&c);mk_input_init(&i,true);
    for(unsigned n=0;n<25000;n++) {
        uint8_t r[16];for(unsigned k=0;k<16;k++)r[k]=(uint8_t)random32();
        size_t len=random32()%17;mk_keyboard_report(&i,r,len);mk_mouse_report(&i,r,len,n);
        mk_step(&i,&c,n,random32()%60,&p);
        CHECK(p.rx>=-32767&&p.ry>=-32767&&p.lx>=-32767&&p.ly>=-32767);
        char text[128];for(unsigned k=0;k<sizeof(text);k++)text[k]=(char)random32();
        MkConfig old=c;unsigned line;bool ok=mk_parse_config(&c,text,random32()%sizeof(text),0,&line);
        if(!ok)CHECK(mk_equal(&c,&old,sizeof(c)));
    }
}
int main(void) {
    movement_and_buttons();aim_and_disconnect();report_and_hotkeys();configs();sampling();malformed_inputs();
    printf("PASS: %u checks (mapping, reports, timing, configuration and deterministic fuzz)\n",checks);
    return 0;
}
