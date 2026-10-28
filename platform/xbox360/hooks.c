/* SPDX-License-Identifier: GPL-3.0-only */
#include "hooks.h"

/* At 0x81Fxxxxx both kernel and XAM are within a PPC relative branch's 32MiB
 * reach. A one-instruction detour avoids relocated save-register calls,
 * scratch-register clobbers and partially written four-instruction branches.
 */
__asm__(".section .text.mk360_tramp,\"ax\"\n.balign 32\n.globl mk_trampolines\nmk_trampolines:\n.space 5*32\n.previous\n");
extern uint32_t mk_trampolines[];
static bool branch(uint32_t from,uint32_t to,uint32_t *out) {
    int32_t delta=(int32_t)(to-from);
    if((delta&3)||delta<-0x02000000||delta>0x01fffffc)return false;
    *out=0x48000000u|((uint32_t)delta&0x03fffffcu);return true;
}
static void sync_code(uint32_t *p) {
    __asm__ volatile("dcbst 0,%0\n sync\n icbi 0,%0\n sync\n isync"::"r"(p):"memory");
}
static bool write_word(uint32_t *p,uint32_t word) {
    uint32_t old=MmQueryAddressProtect((uint32_t)(uintptr_t)p);
    if(!old)return false;
    MmSetAddressProtect(p,4,0x40); /* PAGE_EXECUTE_READWRITE */
    *(volatile uint32_t *)p=word;sync_code(p);
    bool ok=*(volatile uint32_t *)p==word;
    MmSetAddressProtect(p,4,old);
    return ok;
}
bool mk_hook_prepare(MkHook *h,void *src,void *dst,unsigned slot) {
    if(!src||!dst||slot>=5||((uintptr_t)src&3))return false;
    uint32_t word=*(volatile uint32_t *)src,op=word>>26;
    /* Only a position-independent first instruction is accepted. */
    if(op==16||op==18||op==19||word==0||word==0xffffffffu)return false;
    mk_zero(h,sizeof(*h));h->source=src;h->original=word;h->trampoline=mk_trampolines+slot*8;
    uint32_t resume;
    if(!branch((uint32_t)(uintptr_t)src,(uint32_t)(uintptr_t)dst,&h->patch)||
       !branch((uint32_t)(uintptr_t)(h->trampoline+1),(uint32_t)(uintptr_t)src+4,&resume))return false;
    return write_word(h->trampoline,word)&&write_word(h->trampoline+1,resume);
}
bool mk_hook_enable(MkHook *h) {
    if(h->installed||*h->source!=h->original)return false;
    if(!write_word(h->source,h->patch))return false;
    h->installed=true;return true;
}
bool mk_hook_disable(MkHook *h) {
    if(!h->installed)return true;
    /* Preserve any patch another plugin installed after us. */
    if(*h->source!=h->patch)return false;
    if(!write_word(h->source,h->original))return false;
    h->installed=false;return true;
}
