/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MK360_H
#define MK360_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MK360_VERSION "0.1.0-experimental"
#define MK360_CONFIG_LIMIT 8192u
#define MK360_POLL_MS 4u

enum {
    MK_UP=0x0001, MK_DOWN=0x0002, MK_LEFT=0x0004, MK_RIGHT=0x0008,
    MK_START=0x0010, MK_BACK=0x0020, MK_LS=0x0040, MK_RS=0x0080,
    MK_LB=0x0100, MK_RB=0x0200, MK_A=0x1000, MK_B=0x2000,
    MK_X=0x4000, MK_Y=0x8000,
    MK_LT=0x10000, MK_RT=0x20000, MK_MOVE_UP=0x40000,
    MK_MOVE_DOWN=0x80000, MK_MOVE_LEFT=0x100000, MK_MOVE_RIGHT=0x200000
};
typedef struct {
    uint16_t buttons;
    uint8_t lt,rt;
    int16_t lx,ly,rx,ry;
} MkPad;
_Static_assert(sizeof(MkPad)==12,"XINPUT_GAMEPAD layout");

typedef struct {
    int sensitivity;           /* stick units/count at a 4ms sampling interval */
    int ads_percent;
    int deadzone_x,deadzone_y;
    int smoothing;             /* old-sample weight, 0..90 percent */
    int invert_y;
    int enabled;
    int require_xbdm;
    uint32_t key[256];
    uint32_t mouse[3];
} MkConfig;
typedef struct {
    uint8_t keys[256];
    uint8_t mouse_buttons;
    int32_t pending_x,pending_y;
    uint32_t mouse_at;
    int32_t filtered_x,filtered_y;
    bool enabled;
    bool toggle_latch,down_latch,up_latch,reload_latch;
    bool reload_requested;
    int sensitivity_delta;
} MkInput;

void mk_zero(void *p,size_t n);
void mk_copy(void *dst,const void *src,size_t n);
bool mk_equal(const void *a,const void *b,size_t n);
size_t mk_length(const char *s);
bool mk_text_equal(const char *a,const char *b);
void mk_defaults(MkConfig *cfg);
bool mk_parse_config(MkConfig *out,const char *text,size_t length,uint32_t title,unsigned *bad_line);
void mk_input_init(MkInput *input,bool enabled);
bool mk_keyboard_report(MkInput *input,const uint8_t *report,size_t length);
bool mk_mouse_report(MkInput *input,const uint8_t *report,size_t length,uint32_t now);
void mk_release_keyboard(MkInput *input);
void mk_release_mouse(MkInput *input);
void mk_reset_motion(MkInput *input);
void mk_step(MkInput *input,const MkConfig *cfg,uint32_t now,uint32_t elapsed_ms,MkPad *out);
int mk_key_code(const char *name);
uint32_t mk_action(const char *name);

#endif
