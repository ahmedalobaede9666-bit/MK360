/* SPDX-License-Identifier: GPL-3.0-only
 * Experimental 17559 USB boot-HID backend. See docs/HARDWARE_STATUS.md.
 */
#include "usb.h"
#include "hooks.h"

#define SESSION_COUNT 8
#define VIRTUAL_ID 0xa7553952u
#define VIRTUAL_CONTEXT 0x10000005u
enum { NEW=0, PROTOCOL_PENDING=1, READING=2, RETIRED=3 };
typedef struct {
    HidExtension extension;
    UsbDevice *device;
    uint8_t report[64] __attribute__((aligned(32)));
    uint32_t capacity;
    volatile uint32_t phase;
    uint8_t kind,interface_number;
    bool used;
    bool failed;
} Session;
static Session sessions[SESSION_COUNT] __attribute__((aligned(32)));
static volatile uint32_t device_lock;
static uint32_t reports;
static uint8_t virtual_user=255;
static bool bound;
static MkHook hooks[5];

static UsbInterface *(*get_interface)(UsbDevice *);
static UsbEndpoint *(*get_endpoint)(UsbDevice *,int,int,int);
static int (*add_complete)(UsbDevice *,int);
static NtStatus (*open_default)(UsbDevice *,uint32_t *);
static NtStatus (*open_endpoint)(UsbDevice *,int,int,int,int,uint32_t *);
static int (*queue_transfer)(UsbDevice *,void *);
static int (*bind_device)(uint32_t,uint32_t,uint8_t,bool,uint8_t *);

static Session *from_trb(uint32_t address,bool control) {
    for(unsigned k=0;k<SESSION_COUNT;k++) {
        void *p=control?(void *)&sessions[k].extension.control:(void *)&sessions[k].extension.interrupt;
        if((uint32_t)(uintptr_t)p==address&&sessions[k].used)return &sessions[k];
    }
    return 0;
}
static void release_input(Session *s) {
    uint32_t irql=KfAcquireSpinLock(&mk_input_lock);
    if(s->kind==1)mk_release_keyboard(&mk_input);else mk_release_mouse(&mk_input);
    mk_zero(&mk_output,sizeof(mk_output));++mk_packet;
    KfReleaseSpinLock(&mk_input_lock,irql);
}
static void fail_session(Session *s,const char *why) {
    uint32_t irql=KfAcquireSpinLock(&device_lock);
    s->failed=true;s->phase=RETIRED;s->extension.cleanup_done=1;
    KfReleaseSpinLock(&device_lock,irql);
    release_input(s);mk_log(why);
}
static int interrupt_complete(uint32_t trb_address,int32_t status) {
    Session *s=from_trb(trb_address,false);
    if(!s||s->phase!=READING)return 0;
    if(status!=0){fail_session(s,"USB interrupt failed; input released, reconnect/reboot required");return 0;}
    /* The kernel writes the actual byte count back into TRB.length. Never
     * interpret unused bytes from a short/failed transfer as held keys. */
    uint32_t length=s->extension.interrupt.length;
    bool ok=false;uint32_t now=mk_millis();
    uint32_t irql=KfAcquireSpinLock(&mk_input_lock);
    if(length<=s->capacity) {
        if(s->kind==1)ok=mk_keyboard_report(&mk_input,s->report,length);
        else ok=mk_mouse_report(&mk_input,s->report,length,now);
    }
    KfReleaseSpinLock(&mk_input_lock,irql);
    if(ok){irql=KfAcquireSpinLock(&device_lock);++reports;KfReleaseSpinLock(&device_lock,irql);}
    /* Slots and buffers are never freed or reused until reboot. A late USB
     * completion therefore cannot target a new device's allocation. */
    if(s->phase!=READING)return 0;
    s->extension.interrupt.length=s->capacity;s->extension.interrupt.flags=1;
    int result=queue_transfer(s->device,&s->extension.interrupt);
    if(result<0)fail_session(s,"Requeuing USB input failed; held input released");
    return result;
}
static int protocol_complete(uint32_t trb_address,int32_t status) {
    Session *s=from_trb(trb_address,true);
    if(!s||s->phase!=PROTOCOL_PENDING)return 0;
    if(status!=0){fail_session(s,"SET_PROTOCOL(boot) failed; device is not activated");return 0;}
    UsbEndpoint *ep=get_endpoint(s->device,0,3,1);
    if(!ep){fail_session(s,"Missing USB interrupt IN endpoint");return 0;}
    NtStatus st=open_endpoint(s->device,3,ep->address,(int)s->capacity,ep->interval,&s->extension.interrupt.endpoint);
    if(st<0){fail_session(s,"Opening interrupt endpoint failed");return st;}
    UsbTrb *trb=&s->extension.interrupt;
    trb->saved_endpoint=trb->endpoint;trb->callback=(uint32_t)(uintptr_t)interrupt_complete;
    trb->buffer=s->report;trb->length=s->capacity;trb->flags=1;
    __asm__ volatile("sync":::"memory");s->phase=READING;
    mk_log(s->kind==1?"Boot keyboard ready":"Boot mouse ready (3 buttons, relative X/Y)");
    int result=queue_transfer(s->device,trb);
    if(result<0)fail_session(s,"Queuing USB interrupt failed");
    return result;
}
static int add_device(UsbDevice *device) {
    UsbInterface *interface=get_interface(device);
    if(!interface||interface->class_code!=3||interface->subclass!=1||
       (interface->protocol!=1&&interface->protocol!=2))
        return ((int (*)(UsbDevice *))hooks[4].trampoline)(device);
    UsbEndpoint *ep=get_endpoint(device,0,3,1);
    uint32_t capacity=ep?((uint32_t)((ep->max_packet>>8)|(ep->max_packet<<8))&0x7ff):0;
    if(!ep||capacity>64||capacity<(interface->protocol==1?8u:3u))
        return ((int (*)(UsbDevice *))hooks[4].trampoline)(device);
    Session *s=0;uint32_t irql=KfAcquireSpinLock(&device_lock);
    bool duplicate=false;
    for(unsigned k=0;k<SESSION_COUNT;k++)
        if(sessions[k].used&&sessions[k].phase!=RETIRED&&sessions[k].kind==interface->protocol)duplicate=true;
    if(!duplicate)for(unsigned k=0;k<SESSION_COUNT;k++)if(!sessions[k].used){s=&sessions[k];break;}
    if(s) {
        mk_zero(s,sizeof(*s));s->used=true;s->phase=PROTOCOL_PENDING;s->kind=interface->protocol;
        s->device=device;s->capacity=capacity;s->interface_number=interface->interface_number;
        s->extension.device=device;
    }
    KfReleaseSpinLock(&device_lock,irql);
    if(!s){mk_log("Extra HID interface/session limit: passed to native handler");return ((int (*)(UsbDevice *))hooks[4].trampoline)(device);}
    device->driver=&s->extension;
    add_complete(device,0);
    NtStatus st=open_default(device,&s->extension.control.trb.endpoint);
    if(st<0){fail_session(s,"Opening default USB endpoint failed");return st;}
    UsbControl *c=&s->extension.control;
    c->request_type=0x21;c->request=0x0b; /* HID SET_PROTOCOL */
    c->value=0;c->index=(uint16_t)(s->interface_number<<8);c->length=0;
    c->trb.saved_endpoint=c->trb.endpoint;c->trb.callback=(uint32_t)(uintptr_t)protocol_complete;
    c->trb.flags=1;c->trb.buffer=0;c->trb.length=0;
    int result=queue_transfer(device,c);
    if(result<0)fail_session(s,"Queuing SET_PROTOCOL failed");
    return result;
}
static int remove_device(UsbDevice *device) {
    Session *s=0;uint32_t irql=KfAcquireSpinLock(&device_lock);
    for(unsigned k=0;k<SESSION_COUNT;k++)if(sessions[k].used&&device->driver==&sessions[k].extension){s=&sessions[k];break;}
    if(s){s->phase=RETIRED;s->extension.cleanup_done=1;}
    KfReleaseSpinLock(&device_lock,irql);
    if(!s)return ((int (*)(UsbDevice *))hooks[3].trampoline)(device);
    release_input(s);mk_log("USB removed; session quarantined until reboot");
    /* Cancellation/close sequencing is not yet verified on hardware. Do not
     * free/reuse this memory or synchronously close an in-flight endpoint.
     * See HARDWARE_STATUS.md for the resulting hotplug limitation. */
    return 0;
}
static NtStatus read_state(uint32_t context,uint32_t *packet,MkPad *pad,uint32_t *unknown) {
    if(context!=VIRTUAL_CONTEXT)
        return ((NtStatus (*)(uint32_t,uint32_t *,MkPad *,uint32_t *))hooks[0].trampoline)(context,packet,pad,unknown);
    if(!pad)return (NtStatus)0xc000000du;
    uint32_t irql=KfAcquireSpinLock(&mk_input_lock);
    *pad=mk_output;if(packet)*packet=mk_packet;if(unknown)*unknown=0;
    KfReleaseSpinLock(&mk_input_lock,irql);return 0;
}
typedef struct {
    uint8_t type,subtype;uint16_t flags;MkPad pad;uint16_t motor_left,motor_right;uint32_t unknown[3];
} Capabilities;
static uint32_t capabilities(uint32_t unknown,uint32_t user,uint32_t flags,Capabilities *out) {
    uint32_t result=((uint32_t (*)(uint32_t,uint32_t,uint32_t,Capabilities *))hooks[1].trampoline)(unknown,user,flags,out);
    uint32_t irql=KfAcquireSpinLock(&device_lock);bool ours=bound&&(user&0xff)==virtual_user;KfReleaseSpinLock(&device_lock,irql);
    if(result==1167&&ours&&out) {
        mk_zero(out,sizeof(*out));out->type=1;out->subtype=1;
        out->pad.buttons=0xf3ff;out->pad.lt=out->pad.rt=255;
        out->pad.lx=out->pad.ly=out->pad.rx=out->pad.ry=32767;
        return 0;
    }
    return result;
}
static uint32_t set_state(uint32_t user,uint32_t flags,void *state,uint8_t amplitude,uint8_t frequency,uint8_t offset) {
    uint32_t irql=KfAcquireSpinLock(&device_lock);bool ours=bound&&(user&0xff)==virtual_user;KfReleaseSpinLock(&device_lock,irql);
    if(ours)return 0; /* virtual controller has no motors */
    return ((uint32_t (*)(uint32_t,uint32_t,void *,uint8_t,uint8_t,uint8_t))hooks[2].trampoline)(user,flags,state,amplitude,frequency,offset);
}
static bool resolve(Handle module,uint32_t ordinal,void *out) {
    void *address=0;
    if(XexGetProcedureAddress(module,ordinal,&address)<0||!address)return false;
    mk_copy(out,&address,sizeof(address));return true;
}
bool mk_usb_install(void) {
    Handle kernel=0,xam=0;void *read=0,*caps=0,*set=0;
    if(XexGetModuleHandle("xboxkrnl.exe",&kernel)||XexGetModuleHandle("xam.xex",&xam))return false;
    if(!resolve(kernel,744,&get_endpoint)||!resolve(kernel,740,&add_complete)||
       !resolve(kernel,746,&open_default)||!resolve(kernel,747,&open_endpoint)||
       !resolve(kernel,748,&queue_transfer)||!resolve(kernel,486,&read)||
       !resolve(xam,685,&caps)||!resolve(xam,402,&set))return false;
    /* Retail kernel 17559 only. KernelVersion is checked before this call. */
    get_interface=(UsbInterface *(*)(UsbDevice *))(uintptr_t)0x800d8500;
    bind_device=(int (*)(uint32_t,uint32_t,uint8_t,bool,uint8_t *))(uintptr_t)0x816d9060;
    if(*(volatile uint32_t *)(uintptr_t)0x800e4d68!=0x7d8802a6u||
       (*(volatile uint32_t *)(uintptr_t)0x800e4d28&0xffff0000u)!=0x81630000u) {
        mk_log("17559 USB prologue mismatch or conflicting plugin; no hooks applied");return false;
    }
    void *sources[]={read,caps,set,(void *)(uintptr_t)0x800e4d28,(void *)(uintptr_t)0x800e4d68};
    void *targets[]={(void *)read_state,(void *)capabilities,(void *)set_state,(void *)remove_device,(void *)add_device};
    for(unsigned k=0;k<5;k++)if(!mk_hook_prepare(&hooks[k],sources[k],targets[k],k)){mk_log_value("Unsupported hook prologue/protection, index",k);return false;}
    for(unsigned k=0;k<5;k++)if(!mk_hook_enable(&hooks[k])) {
        while(k)mk_hook_disable(&hooks[--k]);
        mk_log("Hook installation failed; rollback requested");return false;
    }
    mk_log("USB/controller hooks installed; connect keyboard and mouse after dashboard loads");
    return true;
}
void mk_usb_counts(uint32_t *keyboards,uint32_t *mice,uint32_t *packets,uint32_t *user) {
    uint32_t irql=KfAcquireSpinLock(&device_lock);*keyboards=*mice=0;
    for(unsigned k=0;k<SESSION_COUNT;k++)if(sessions[k].used&&sessions[k].phase==READING) {
        if(sessions[k].kind==1)++*keyboards;else ++*mice;
    }
    *packets=reports;*user=virtual_user;KfReleaseSpinLock(&device_lock,irql);
}
void mk_usb_tick(void) {
    uint32_t keyboards,mice,packets,user;mk_usb_counts(&keyboards,&mice,&packets,&user);
    if((keyboards||mice)&&!bound) {
        uint8_t next=255;
        int result=bind_device(VIRTUAL_ID,VIRTUAL_CONTEXT,0,false,&next);
        if(result>=0&&next<4){uint32_t irql=KfAcquireSpinLock(&device_lock);virtual_user=next;bound=true;KfReleaseSpinLock(&device_lock,irql);mk_log_value("Virtual player registered (zero based)",next);}
    } else if(!keyboards&&!mice&&bound) {
        bind_device(VIRTUAL_ID,VIRTUAL_CONTEXT,0,true,0);
        uint32_t irql=KfAcquireSpinLock(&device_lock);bound=false;virtual_user=255;KfReleaseSpinLock(&device_lock,irql);mk_log("Virtual player removed");
    }
}
void mk_usb_clear_for_title(void) {
    /* Re-register through XAM after every title transition. The USB stack,
     * system thread and non-title buffers are kept alive. */
    if(bound)bind_device(VIRTUAL_ID,VIRTUAL_CONTEXT,0,true,0);
    uint32_t dl=KfAcquireSpinLock(&device_lock);bound=false;virtual_user=255;KfReleaseSpinLock(&device_lock,dl);
    uint32_t irql=KfAcquireSpinLock(&mk_input_lock);
    mk_release_keyboard(&mk_input);mk_release_mouse(&mk_input);mk_zero(&mk_output,sizeof(mk_output));++mk_packet;
    KfReleaseSpinLock(&mk_input_lock,irql);
}
