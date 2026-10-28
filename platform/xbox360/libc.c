/* SPDX-License-Identifier: GPL-3.0-only
 * Only memory helpers the freestanding compiler may synthesize.
 */
#include "mk360.h"
void *memcpy(void *dst,const void *src,size_t n) { mk_copy(dst,src,n);return dst; }
void *memset(void *dst,int value,size_t n) {uint8_t *p=dst;while(n--)*p++=(uint8_t)value;return dst;}
void *memmove(void *dst,const void *src,size_t n) {
    uint8_t *d=dst;const uint8_t *s=src;
    if((uintptr_t)d<(uintptr_t)s)while(n--)*d++=*s++;
    else while(n){--n;d[n]=s[n];}
    return dst;
}
int memcmp(const void *a,const void *b,size_t n) {
    const uint8_t *x=a,*y=b;while(n--){if(*x!=*y)return (int)*x-(int)*y;++x;++y;}return 0;
}
