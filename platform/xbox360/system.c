/* SPDX-License-Identifier: GPL-3.0-only */
#include "kernel.h"

static volatile uint32_t log_lock;
static char log_text[3072];
static size_t log_used;
static const char config_path[]="\\Device\\Harddisk0\\Partition1\\MK360\\MK360.ini";
static const char status_path[]="\\Device\\Harddisk0\\Partition1\\MK360\\MK360.log";

uint32_t mk_millis(void) { return (uint32_t)(KeQueryInterruptTime()/10000u); }
void mk_sleep(uint32_t ms) {
    int64_t interval=-(int64_t)ms*10000;
    KeDelayExecutionThread(0,0,&interval);
}
void mk_log(const char *text) {
    uint32_t irql=KfAcquireSpinLock(&log_lock);
    size_t n=mk_length(text);if(n>160)n=160;
    if(log_used+n+2>=sizeof(log_text)) {
        /* Retain the tail without ever allocating in a USB completion. */
        size_t drop=sizeof(log_text)/2;
        while(drop<log_used&&log_text[drop]!='\n')++drop;
        if(drop<log_used)++drop;
        for(size_t k=drop;k<log_used;k++)log_text[k-drop]=log_text[k];
        log_used-=drop;
    }
    mk_copy(log_text+log_used,text,n);log_used+=n;log_text[log_used++]='\n';log_text[log_used]=0;
    KfReleaseSpinLock(&log_lock,irql);
}
static char *put(char *out,const char *s) { while(*s)*out++=*s++;return out; }
static char *hex(char *out,uint32_t value) {
    static const char digits[]="0123456789ABCDEF";
    for(int shift=28;shift>=0;shift-=4)*out++=digits[(value>>shift)&15];
    return out;
}
static char *decimal(char *out,uint32_t value) {
    char temp[10];unsigned n=0;do{temp[n++]=(char)('0'+value%10);value/=10;}while(value);
    while(n)*out++=temp[--n];
    return out;
}
void mk_log_value(const char *label,uint32_t value) {
    char buffer[192],*p=buffer;
    size_t n=mk_length(label);if(n>160)n=160;
    mk_copy(p,label,n);p+=n;p=put(p," 0x");p=hex(p,value);*p=0;mk_log(buffer);
}
static NtStatus open_file(const char *path,bool write,Handle *file) {
    AnsiString name={(uint16_t)mk_length(path),(uint16_t)(mk_length(path)+1),path};
    ObjectAttributes attr={0,&name,0x40};IoStatus io;
    return NtCreateFile(file,(write?0x40000000u:0x80000000u)|0x100000u,&attr,&io,0,0,3,write?5:1,0x60);
}
bool mk_load_config(MkConfig *config,uint32_t title) {
    Handle file;NtStatus st=open_file(config_path,false,&file);
    if(st<0){mk_log("MK360.ini unavailable; keeping current/default settings");return false;}
    char text[MK360_CONFIG_LIMIT+1];IoStatus io={0,0};
    st=NtReadFile(file,0,0,0,&io,text,sizeof(text),0);NtClose(file);
    if(st<0||io.information>MK360_CONFIG_LIMIT){mk_log("Config read failed or exceeds 8192 bytes");return false;}
    unsigned line=0;
    if(!mk_parse_config(config,text,io.information,title,&line)){mk_log_value("Invalid config, line",line);return false;}
    mk_log("Settings loaded");return true;
}
void mk_write_status(const char *phase,uint32_t title,uint32_t user,uint32_t keyboards,uint32_t mice,uint32_t packets,int sensitivity) {
    char text[4096],*p=text;bool enabled;
    uint32_t irql=KfAcquireSpinLock(&mk_input_lock);enabled=mk_input.enabled;KfReleaseSpinLock(&mk_input_lock,irql);
    p=put(p,"MK360 " MK360_VERSION "\r\nphase=");p=put(p,phase);
    p=put(p,"\r\ntitle_id=");p=hex(p,title);
    p=put(p,"\r\nplayer_index=");p=decimal(p,user);
    p=put(p,"\r\nkeyboards=");p=decimal(p,keyboards);p=put(p,"\r\nmice=");p=decimal(p,mice);
    p=put(p,"\r\nusb_reports=");p=decimal(p,packets);p=put(p,"\r\nmapping_enabled=");p=decimal(p,enabled?1:0);
    p=put(p,"\r\nsensitivity=");p=decimal(p,(uint32_t)sensitivity);
    p=put(p,"\r\nhardware_tested=no\r\n\r\nEvents:\n");
    irql=KfAcquireSpinLock(&log_lock);mk_copy(p,log_text,log_used);p+=log_used;KfReleaseSpinLock(&log_lock,irql);
    Handle file;IoStatus io;
    if(open_file(status_path,true,&file)>=0){NtWriteFile(file,0,0,0,&io,text,(uint32_t)(p-text),0);NtClose(file);}
}
