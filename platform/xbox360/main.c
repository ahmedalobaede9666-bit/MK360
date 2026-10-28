/* SPDX-License-Identifier: GPL-3.0-only */
#include "kernel.h"

volatile uint32_t mk_input_lock;
MkInput mk_input;
MkPad mk_output;
uint32_t mk_packet;
static MkConfig config;

static bool tray_open(void) {
    uint8_t request[16] __attribute__((aligned(16)))={0};
    uint8_t reply[16] __attribute__((aligned(16)))={0};
    request[0]=0x0a;HalSendSMCMessage(request,reply);return reply[1]==0x60;
}
static uint32_t worker(void *context) {
    (void)context;
    mk_log("MK360 boot: open source C/system DLL build");
    mk_defaults(&config);mk_input_init(&mk_input,config.enabled!=0);
    mk_sleep(2000); /* allow earlier DashLaunch plugins and storage to initialize */
    uint32_t title=XamGetCurrentTitleId();mk_load_config(&config,title);mk_input.enabled=config.enabled!=0;
    if(tray_open()) {
        mk_log("Disc tray open: recovery bypass, no hooks installed");
        mk_write_status("bypassed",title,255,0,0,0,config.sensitivity);return 0;
    }
    if(!XboxKrnlVersion||XboxKrnlVersion->build!=17559) {
        mk_log("Unsupported kernel: this version only accepts retail build 17559");
        mk_write_status("unsupported_kernel",title,255,0,0,0,config.sensitivity);return 0;
    }
    Handle xbdm=0;
    if(config.require_xbdm&&XexGetModuleHandle("xbdm.xex",&xbdm)!=0) {
        mk_log("xbdm.xex not loaded before MK360; hardware hooks disabled (upstream USB freeze prerequisite)");
        mk_write_status("xbdm_required",title,255,0,0,0,config.sensitivity);return 0;
    }
    if(!mk_usb_install()) {
        mk_log("Hardware backend refused initialization");
        mk_write_status("hook_init_failed",title,255,0,0,0,config.sensitivity);return 0;
    }
    uint32_t last=mk_millis(),next_status=last;bool first=true;
    for(;;) {
        uint32_t now=mk_millis(),current=XamGetCurrentTitleId();
        if(current!=title) {
            title=current;mk_log_value("Title changed",title);mk_usb_clear_for_title();
            MkConfig next;mk_defaults(&next);mk_load_config(&next,title);config=next;
            uint32_t irql=KfAcquireSpinLock(&mk_input_lock);
            mk_input.enabled=config.enabled!=0;mk_input.sensitivity_delta=0;
            KfReleaseSpinLock(&mk_input_lock,irql);
        }
        mk_usb_tick();
        uint32_t irql=KfAcquireSpinLock(&mk_input_lock);
        MkPad next;mk_step(&mk_input,&config,now,(uint32_t)(now-last),&next);
        if(!mk_equal(&next,&mk_output,sizeof(next))){mk_output=next;++mk_packet;}
        bool reload=mk_input.reload_requested;mk_input.reload_requested=false;
        int sensitivity=config.sensitivity+mk_input.sensitivity_delta;
        KfReleaseSpinLock(&mk_input_lock,irql);
        if(reload) {
            MkConfig next_config=config;
            if(mk_load_config(&next_config,title)) {
                config=next_config;irql=KfAcquireSpinLock(&mk_input_lock);
                mk_input.enabled=config.enabled!=0;mk_input.sensitivity_delta=0;mk_reset_motion(&mk_input);
                KfReleaseSpinLock(&mk_input_lock,irql);
            }
        }
        if(first||(int32_t)(now-next_status)>=0) {
            uint32_t k,m,p,u;mk_usb_counts(&k,&m,&p,&u);
            mk_write_status("running_experimental",title,u,k,m,p,sensitivity);
            next_status=now+5000;first=false;
        }
        last=now;mk_sleep(MK360_POLL_MS);
    }
}
static void thread_start(void *entry,void *context) {
    uint32_t result=((uint32_t (*)(void *))entry)(context);ExTerminateThread(result);
}
uint32_t DllMain(Handle module,uint32_t reason,void *reserved) {
    (void)module;(void)reserved;
    if(reason==1) {
        Handle thread=0;
        /* EX_CREATE_FLAG_SYSTEM=0x2; title-execution flag 0x100 is absent. */
        NtStatus status=ExCreateThread(&thread,64*1024,0,(void *)thread_start,(void *)worker,0,2);
        if(status<0)return 0;
        NtClose(thread);
    }
    /* Installation/removal is by boot configuration, never a live unload. */
    return 1;
}
