/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Derived from exlaunch f9f4b0dd07b68f97958cb9c79228bbca22ca80d5.
 * Configure module naming and runtime-skin build modes.
 * See THIRD_PARTY_NOTICES.md and licenses/source-inventory.json.
 */
#pragma once

#include "common.hpp"

#define EXL_MODULE_NAME "SilkModLoader"

#define EXL_DEBUG
#define EXL_USE_FAKEHEAP

/*
 * Safe hardware probe. The native-hook implementation remains in the source
 * tree, but is disabled until the target process grants exlaunch's
 * svcMapProcessMemory capability. The previous attempt crashed while
 * installing the trampoline, before any Unity code was reached.
 *
 * A deliberate runtime-skin build supplies SILKMODLOADER_RUNTIME_SKIN from
 * config.mk/Make. That build must use the full exlaunch initialization path
 * because the external TK2D redirect calls back into the loader.
 */
#if defined(SILKMODLOADER_RUNTIME_SKIN)
#define SILKMODLOADER_ENABLE_RUNTIME_SKIN
#else
#define SILKMODLOADER_MODULE_PROBE
#endif

/*
 * The loader implementation is present but intentionally opt-in. Supplying
 * the build flag makes the existing external tk2d Init stub read
 * rom:/SilkModLoader/Mods/Skin and call the verified Unity
 * LoadImage_Injected path. Keep it disabled until a deliberate hardware run;
 * the default build remains the known-safe probe.
 */
/* #define SILKMODLOADER_RUNTIME_SKIN */

/*
#define SILKMODLOADER_NATIVE_HOOK_PROBE
*/

/*
#define EXL_SUPPORTS_REBOOTPAYLOAD
*/

namespace exl::setting {
    /* The skin probe reads a roughly 10 MiB PNG through the module heap. */
#ifdef SILKMODLOADER_ENABLE_RUNTIME_SKIN
    constexpr size_t HeapSize = 0x00C00000;
#else
    constexpr size_t HeapSize = 0x5000;
#endif

    /* How large the JIT area will be for hooks. */
    constexpr size_t JitSize = 0x1000;

    /* How large the area will be inline hook pool. */
    constexpr size_t InlinePoolSize = 0x1000;

    /* How large the formatting buffer should be for logging. The buffer will be on the stack. */
    constexpr size_t LogBufferSize = 512;

    /* Sanity checks. */
    static_assert(ALIGN_UP(JitSize, PAGE_SIZE) == JitSize, "");
    static_assert(ALIGN_UP(InlinePoolSize, PAGE_SIZE) == InlinePoolSize, "");
}
