/*
 * Copyright 2021 Alyssa Rosenzweig
 * Copyright 2020 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>

#include "util/macros.h"

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
enum agx_compiler_dbg {
   /* bit 0 unused */
   AGX_DBG_SHADERS     = BITFIELD_BIT(1),
   AGX_DBG_SHADERDB    = BITFIELD_BIT(2),
   AGX_DBG_VERBOSE     = BITFIELD_BIT(3),
   AGX_DBG_INTERNAL    = BITFIELD_BIT(4),
   AGX_DBG_NOVALIDATE  = BITFIELD_BIT(5),
   AGX_DBG_NOOPT       = BITFIELD_BIT(6),
   AGX_DBG_WAIT        = BITFIELD_BIT(7),
   AGX_DBG_NOPREAMBLE  = BITFIELD_BIT(8),
   AGX_DBG_DEMAND      = BITFIELD_BIT(9),
   AGX_DBG_NOSCHED     = BITFIELD_BIT(10),
   AGX_DBG_SPILL       = BITFIELD_BIT(11),
   AGX_DBG_NOPROMOTE   = BITFIELD_BIT(12),
   AGX_DBG_LATECOV     = BITFIELD_BIT(13),
   AGX_DBG_REFUSE      = BITFIELD_BIT(14),
   AGX_DBG_BINARIES    = BITFIELD_BIT(15),
};
/* clang-format on */

uint64_t agx_get_compiler_debug(void);

/* Qualified on G16G; ASAHI_G16_HOST_BLEND_SRC=0 restores per-pixel evaluation
 * of uniform-only inline-blend fragment arithmetic. */
bool agx_g16_host_blend_src_enabled(void);
/* Qualified on G16G; ASAHI_G16_BORDER_TWIN=0 keeps the custom-border emulation in
 * every G16G texture sample instead of compiling border-free twins. */
bool agx_g16_border_twin_enabled(void);

/* Why the last agx_compile_shader_nir() on this thread returned false
 * ("" when it succeeded), for the driver's "cannot encode" error. */
const char *agx_last_compile_refusal(void);

/* Fragment-structure code generation choices that remain selectable. */
enum agx_fs_structure_flag {
   /* Compile the colour epilog into the fragment shader at link time, as
    * Apple's compiler blends inline. Honeykrisp keeps the main shader's NIR
    * and backend key; at fast-link it appends the draw's epilog, turns every
    * export / load_exported pair into SSA values and compiles one fragment
    * program. It falls back to the split main + epilog whenever the
    * monolithic compile would change any uniform, rodata, varying or
    * preamble layout the draw path takes from the API shader, or fails to
    * compile. */
   AGX_FS_STRUCTURE_INLINE_BLEND = BITFIELD_BIT(4),
};

uint32_t agx_get_fs_structure_flags(void);

#ifdef __cplusplus
} /* extern C */
#endif
