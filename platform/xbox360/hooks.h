/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MK_HOOKS_H
#define MK_HOOKS_H
#include "kernel.h"
typedef struct {uint32_t *source;uint32_t original,patch;uint32_t *trampoline;bool installed;} MkHook;
bool mk_hook_prepare(MkHook *hook,void *source,void *destination,unsigned slot);
bool mk_hook_enable(MkHook *hook);
bool mk_hook_disable(MkHook *hook);
#endif
