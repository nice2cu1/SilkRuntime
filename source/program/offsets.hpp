/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Derived from exlaunch f9f4b0dd07b68f97958cb9c79228bbca22ca80d5.
 * Add Build-ID-bound Silksong addresses and instruction checks.
 * See THIRD_PARTY_NOTICES.md and licenses/source-inventory.json.
 */
#pragma once

#include <common.hpp>
#include <tuple>

#include "version.hpp"
#include "hook/hook_target.hpp"
#include "lib/reloc/reloc.hpp"

namespace silkmodloader::game {

/*
 * Build-ID-bound IL2CPP RVAs for the currently verified Silksong Switch
 * executable.  These are deliberately data-only in the probe stage: no
 * address from this table is called or hooked until its ABI and lifecycle
 * have been verified on hardware.
 */
struct Il2CppOffsets {
    uintptr_t tk2dSpriteCollectionData_InitMaterialIds;
    uintptr_t tk2dSpriteCollectionData_Init;
    uintptr_t imageConversion_LoadImage_ByteArray;
    uintptr_t imageConversion_LoadImage_Injected;
    uintptr_t material_get_mainTexture;
    uintptr_t material_set_mainTexture;
    uintptr_t texture_get_width;
    uintptr_t texture_get_height;
    uintptr_t texture2d_get_format;
    uintptr_t object_get_name;
    uintptr_t sprite_get_texture;
    uintptr_t spriteRenderer_set_sprite;
    uintptr_t uiImage_set_sprite;
};

inline constexpr char kVerifiedMainBuildIdText[] =
    "fc9ea4ccc955d5799f37752b2d730b31";

inline constexpr Il2CppOffsets kVerifiedIl2CppOffsets = {
    .tk2dSpriteCollectionData_InitMaterialIds = 0x5b587b0,
    .tk2dSpriteCollectionData_Init = 0x5b58b20,
    .imageConversion_LoadImage_ByteArray = 0x6042890,
    .imageConversion_LoadImage_Injected = 0x6042820,
    .material_get_mainTexture = 0x5f82800,
    .material_set_mainTexture = 0x5f82980,
    .texture_get_width = 0x5fafd00,
    .texture_get_height = 0x5fafe30,
    .texture2d_get_format = 0x5fb1ac0,
    .object_get_name = 0x5ff4860,
    .sprite_get_texture = 0x5f426f0,
    .spriteRenderer_set_sprite = 0x5f3f650,
    .uiImage_set_sprite = 0x6182740,
};

/*
 * Offline-verified call-site candidate for the first external patch plan.
 * The instruction at main+0x5b582a8 is BL main+0x5b58b20 in the exact
 * fc9ea4... text.bin.  This is metadata only: no instruction is written by
 * the current runtime.
 */
inline constexpr hook::HookTarget kVerifiedTk2dInitCallTarget = {
    .name = "tk2dSpriteCollectionData::Init call-site",
    .callSiteOffset = 0x5b582a8,
    .originalFunctionOffset = 0x5b58b20,
    .expectedInstruction = 0x9400021e,
    .branchType = hook::BranchType::BL,
};

/* Two direct calls to Material.set_mainTexture are present in the exact
 * verified main NSO.  The stage script patches both call sites to the
 * external observer; the first one is used here as the runtime bind check. */
inline constexpr hook::HookTarget kVerifiedMaterialSetMainTextureCallTarget = {
    .name = "Material::set_mainTexture call-site",
    .callSiteOffset = 0x5b58f60,
    .originalFunctionOffset = 0x5f82980,
    .expectedInstruction = 0x9410a688,
    .branchType = hook::BranchType::BL,
};

inline constexpr hook::HookTarget kVerifiedSpriteRendererSetSpriteCallTarget = {
    .name = "SpriteRenderer::set_sprite call-site",
    .callSiteOffset = 0x21acb98,
    .originalFunctionOffset = 0x5f3f650,
    .expectedInstruction = 0x94f64aae,
    .branchType = hook::BranchType::BL,
};

inline constexpr hook::HookTarget kVerifiedUIImageSetSpriteCallTarget = {
    .name = "UI.Image::set_sprite call-site",
    .callSiteOffset = 0x21acc54,
    .originalFunctionOffset = 0x6182740,
    .expectedInstruction = 0x94ff56bb,
    .branchType = hook::BranchType::BL,
};

} // namespace silkmodloader::game

namespace exl::reloc {
    using VersionType = util::UserVersion;

    template<VersionType Version, impl::LookupEntry... Entries>
    using UserTableType = VersionedTable<Version, Entries...>;

    using UserTableSet = TableSet<VersionType 
        /*
        // This feature allows you to specify symbols in your executable to resolve with module+offset pairs.
        // They are packed up and sorted at compile time so they can be efficiently looked up.
        // The `exl::reloc::GetLookupTable` API is provided if you want to look up entries in the table explicitly.
        // Examples of tables:
        UserTableType<VersionType::DEFAULT,
        //    Module offset is relative to.     Offset within module.       Symbol name.
            { util::ModuleIndex::Main,          0x6961,                     "example1" },
            { util::ModuleIndex::Sdk,           0x6962,                     "example2" },
            { util::ModuleIndex::Rtld,          0x6963,                     "example3" }
        >,

        // In addition, you can specify multiple tables and select the correct one at runtime. This allows you to
        // support multiple versions/variations of a game in one executable. See version.hpp to see how to implement
        // multiple supported versions.
        UserTableType<VersionType::OTHER,
        //    Module offset is relative to.     Offset within module.       Symbol name.
            { util::ModuleIndex::Main,          0x4201,                     "example1" },
            { util::ModuleIndex::Sdk,           0x4202,                     "example2" }
        >
        */
    >;
}
