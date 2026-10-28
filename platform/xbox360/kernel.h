/* SPDX-License-Identifier: GPL-3.0-only
 * Minimal declarations from public reverse engineering; no XDK headers.
 */
#ifndef MK_KERNEL_H
#define MK_KERNEL_H
#include "mk360.h"
typedef int32_t NtStatus;
typedef uint32_t Handle;
typedef struct { uint16_t length,maximum_length;const char *pointer; } AnsiString;
typedef struct { Handle root;const AnsiString *name;uint32_t attributes; } ObjectAttributes;
typedef struct { NtStatus status;uint32_t information; } IoStatus;
typedef struct { uint8_t major,minor;uint16_t build;uint8_t qfe,reserved; } KernelVersion;
extern const KernelVersion *const XboxKrnlVersion;
NtStatus ExCreateThread(Handle *,uint32_t,uint32_t *,void *,void *,void *,uint32_t);
void ExTerminateThread(uint32_t);
NtStatus KeDelayExecutionThread(uint32_t,uint32_t,const int64_t *);
uint64_t KeQueryInterruptTime(void);
uint32_t KfAcquireSpinLock(volatile uint32_t *);
void KfReleaseSpinLock(volatile uint32_t *,uint32_t);
void HalSendSMCMessage(const void *,void *);
uint32_t XexGetModuleHandle(const char *,Handle *);
NtStatus XexGetProcedureAddress(Handle,uint32_t,void **);
uint32_t MmQueryAddressProtect(uint32_t);
void MmSetAddressProtect(void *,uint32_t,uint32_t);
NtStatus NtClose(Handle);
NtStatus NtCreateFile(Handle *,uint32_t,const ObjectAttributes *,IoStatus *,const uint64_t *,uint32_t,uint32_t,uint32_t,uint32_t);
NtStatus NtReadFile(Handle,Handle,void *,void *,IoStatus *,void *,uint32_t,const uint64_t *);
NtStatus NtWriteFile(Handle,Handle,void *,void *,IoStatus *,const void *,uint32_t,const uint64_t *);
uint32_t XamGetCurrentTitleId(void);

uint32_t mk_millis(void);
void mk_sleep(uint32_t ms);
void mk_log(const char *text);
void mk_log_value(const char *label,uint32_t value);
void mk_write_status(const char *phase,uint32_t title,uint32_t user,uint32_t keyboards,uint32_t mice,uint32_t packets,int sensitivity);
bool mk_load_config(MkConfig *config,uint32_t title);

extern volatile uint32_t mk_input_lock;
extern MkInput mk_input;
extern MkPad mk_output;
extern uint32_t mk_packet;
bool mk_usb_install(void);
void mk_usb_tick(void);
void mk_usb_counts(uint32_t *keyboards,uint32_t *mice,uint32_t *packets,uint32_t *user);
void mk_usb_clear_for_title(void);
#endif
