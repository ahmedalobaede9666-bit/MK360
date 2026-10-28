/* SPDX-License-Identifier: GPL-3.0-only
 * USB ABI/offsets based on EinTim23/hiddriver360 and HB-Agadir/x360remap.
 */
#ifndef MK_USB_H
#define MK_USB_H
#include "kernel.h"
typedef struct UsbDevice UsbDevice;
typedef struct {
    uint32_t endpoint,callback,saved_endpoint;
    uint8_t padding[4],flags,controller_index,pad2,endpoint_index;
    void *buffer;uint32_t length;
} UsbTrb;
typedef struct {
    UsbTrb trb;uint8_t padding[4];
    uint8_t request_type,request;uint16_t value,index,length;
} UsbControl;
typedef struct {
    UsbDevice *device;
    UsbTrb interrupt;
    uint8_t gap20[4];
    UsbControl control;
    uint8_t gap4c[4];uint32_t cleanup_handler;uint8_t gap54[24];uint32_t queue;
    uint8_t one,one2,flag,zero,cleanup_done,init_pending,zero2,device_type,zero3,zero4;
} HidExtension;
struct UsbDevice { HidExtension *driver; };
typedef struct {
    uint8_t length,type,interface_number,alternate,endpoints,class_code,subclass,protocol,string;
} UsbInterface;
typedef struct __attribute__((packed)) {
    uint8_t length,type,address,attributes;uint16_t max_packet;uint8_t interval;
} UsbEndpoint;
_Static_assert(sizeof(void*)==4,"Xbox 360 pointers");
_Static_assert(sizeof(UsbTrb)==28,"USB transfer ABI");
_Static_assert(offsetof(HidExtension,interrupt)==4,"interrupt callback offset");
_Static_assert(offsetof(HidExtension,control)==36,"control callback offset");
_Static_assert(offsetof(HidExtension,cleanup_handler)==80,"USB cleanup offset");
_Static_assert(offsetof(HidExtension,cleanup_done)==116,"USB cleanup flag offset");
#endif
