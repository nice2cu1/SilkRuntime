/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Derived from exlaunch f9f4b0dd07b68f97958cb9c79228bbca22ca80d5.
 * Add Build-ID validation, hook callbacks and skin loading.
 * See THIRD_PARTY_NOTICES.md and licenses/source-inventory.json.
 */
#include "lib.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "offsets.hpp"
#include "hook/hook_runtime.hpp"

#if !defined(SILKMODLOADER_SVC_ONLY_BOOTSTRAP) || defined(SILKMODLOADER_MODULE_PROBE)
namespace {

constexpr std::string_view kLoaderVersion = "0.1.0-dev";

struct BuildId {
    std::array<std::uint8_t, 32> bytes{};
    std::size_t size{};
};

[[maybe_unused]] BuildId FindGnuBuildId(const exl::util::ModuleInfo& module) {
    constexpr std::uint32_t kNtGnuBuildId = 3;
    constexpr std::size_t kNoteHeaderSize = 12;
    const auto start = module.m_Rodata.m_Start;
    const auto size = module.m_Rodata.m_Size;

    for (std::size_t offset = 0; offset + kNoteHeaderSize <= size; offset += 4) {
        const auto* note = reinterpret_cast<const std::uint32_t*>(start + offset);
        const std::uint32_t nameSize = note[0];
        const std::uint32_t descSize = note[1];
        const std::uint32_t type = note[2];
        if (nameSize != 4 || type != kNtGnuBuildId || descSize == 0 || descSize > 32) {
            continue;
        }

        const std::size_t alignedNameSize = (nameSize + 3U) & ~3U;
        const std::size_t totalSize = kNoteHeaderSize + alignedNameSize + descSize;
        if (offset + totalSize > size) {
            continue;
        }

        const auto* name = reinterpret_cast<const char*>(start + offset + kNoteHeaderSize);
        if (std::memcmp(name, "GNU", 4) != 0) {
            continue;
        }

        BuildId result{};
        result.size = descSize;
        std::memcpy(result.bytes.data(), name + alignedNameSize, descSize);
        return result;
    }

    return {};
}

[[maybe_unused]] void FormatBuildId(const BuildId& buildId, char* output, std::size_t outputSize) {
    constexpr char kHex[] = "0123456789abcdef";
    if (outputSize == 0) {
        return;
    }
    if (buildId.size == 0 || outputSize < buildId.size * 2 + 1) {
        output[0] = '\0';
        return;
    }

    for (std::size_t i = 0; i < buildId.size; ++i) {
        output[i * 2] = kHex[buildId.bytes[i] >> 4];
        output[i * 2 + 1] = kHex[buildId.bytes[i] & 0x0f];
    }
    output[buildId.size * 2] = '\0';
}

const char* ModuleIndexName(exl::util::ModuleIndex index) {
    using exl::util::ModuleIndex;
    switch (index) {
        case ModuleIndex::Rtld: return "rtld";
        case ModuleIndex::Main: return "main";
        case ModuleIndex::Subsdk0: return "subsdk0";
        case ModuleIndex::Subsdk1: return "subsdk1";
        case ModuleIndex::Subsdk2: return "subsdk2";
        case ModuleIndex::Subsdk3: return "subsdk3";
        case ModuleIndex::Subsdk4: return "subsdk4";
        case ModuleIndex::Subsdk5: return "subsdk5";
        case ModuleIndex::Subsdk6: return "subsdk6";
        case ModuleIndex::Subsdk7: return "subsdk7";
        case ModuleIndex::Subsdk8: return "subsdk8";
        case ModuleIndex::Subsdk9: return "subsdk9";
        case ModuleIndex::Sdk: return "sdk";
        default: return "unknown";
    }
}

[[maybe_unused]] void LogModules() {
    using exl::util::ModuleIndex;
    for (int value = static_cast<int>(ModuleIndex::Start);
         value < static_cast<int>(ModuleIndex::End); ++value) {
        const auto index = static_cast<ModuleIndex>(value);
        if (!exl::util::HasModule(index)) {
            continue;
        }

        const auto& module = exl::util::GetModuleInfo(index);
        const auto buildId = FindGnuBuildId(module);
        char buildIdText[65]{};
        FormatBuildId(buildId, buildIdText, sizeof(buildIdText));

        char mappedName[exl::util::ModuleInfo::s_ModulePathLengthMax + 1]{};
        exl::util::CopyString(mappedName, module.GetModuleName());
        Logging.Log("[Module] %s mapped-name=%s", ModuleIndexName(index),
                    mappedName[0] == '\0' ? "<none>" : mappedName);
        Logging.Log("[Module] base=%016lx text=%016lx size=%016lx",
                    module.m_Total.m_Start, module.m_Text.m_Start, module.m_Text.m_Size);
        Logging.Log("[Module] rodata=%016lx size=%016lx data=%016lx size=%016lx",
                    module.m_Rodata.m_Start, module.m_Rodata.m_Size,
                    module.m_Data.m_Start, module.m_Data.m_Size);
        Logging.Log("[Module] build-id=%s",
                    buildIdText[0] == '\0' ? "<not found>" : buildIdText);
    }
}

class ProbeLine {
public:
    void Append(std::string_view text) {
        const auto available = sizeof(m_Buffer) - m_Size;
        const auto count = text.size() < available ? text.size() : available;
        if (count != 0) {
            std::memcpy(m_Buffer + m_Size, text.data(), count);
            m_Size += count;
        }
    }

    void AppendHex(std::uintptr_t value) {
        constexpr char kHex[] = "0123456789abcdef";
        Append("0x");
        for (int shift = 60; shift >= 0; shift -= 4) {
            const auto digit = static_cast<unsigned>((value >> shift) & 0xfU);
            Append(std::string_view(&kHex[digit], 1));
        }
    }

    void AppendBuildId(const BuildId& buildId) {
        constexpr char kHex[] = "0123456789abcdef";
        if (buildId.size == 0) {
            Append("<not-found>");
            return;
        }
        for (std::size_t i = 0; i < buildId.size; ++i) {
            const auto byte = buildId.bytes[i];
            Append(std::string_view(&kHex[byte >> 4], 1));
            Append(std::string_view(&kHex[byte & 0xfU], 1));
        }
    }

    void Flush() {
        svcOutputDebugString(m_Buffer, m_Size);
        m_Size = 0;
    }

private:
    char m_Buffer[320]{};
    std::size_t m_Size{};
};

#ifdef SILKMODLOADER_NATIVE_HOOK_PROBE
std::uintptr_t ReadU64(std::uintptr_t address) {
    return *reinterpret_cast<const std::uintptr_t*>(address);
}

std::uint32_t ReadU32(std::uintptr_t address) {
    return *reinterpret_cast<const std::uint32_t*>(address);
}

bool IsKnightCollection(void* self) {
    if (self == nullptr) {
        return false;
    }

    const auto name = ReadU64(reinterpret_cast<std::uintptr_t>(self) + 0x88);
    if (name == 0 || ReadU32(name + 0x10) != 6) {
        return false;
    }

    const auto* chars = reinterpret_cast<const std::uint16_t*>(name + 0x14);
    constexpr std::uint16_t kKnight[] = {'K', 'n', 'i', 'g', 'h', 't'};
    for (std::size_t index = 0; index < 6; ++index) {
        if (chars[index] != kKnight[index]) {
            return false;
        }
    }
    return true;
}

std::uint32_t ReadArrayLength(std::uintptr_t array) {
    return array == 0 ? 0 : ReadU32(array + 0x18);
}

#ifdef SILKMODLOADER_NATIVE_HOOK_PROBE
HOOK_DEFINE_TRAMPOLINE(Tk2dInitObserverHook) {
    static void Callback(void* self, const void* method) {
        Orig(self, method);

        if (!IsKnightCollection(self)) {
            return;
        }

        static bool emitted = false;
        if (emitted) {
            return;
        }
        emitted = true;

        const auto base = reinterpret_cast<std::uintptr_t>(self);
        const auto materials = ReadU64(base + 0x48);
        const auto textures = ReadU64(base + 0x60);

        ProbeLine line;
        line.Append("[NativeHook] Knight Init original returned; this=");
        line.AppendHex(base);
        line.Append(" materials=");
        line.AppendHex(ReadArrayLength(materials));
        line.Append(" textures=");
        line.AppendHex(ReadArrayLength(textures));
        line.Append(" (read-only observer)\n");
        line.Flush();
    }
};
#endif

bool InstallNativeHookIfSupported() {
    using exl::util::ModuleIndex;
    const auto mainIndex = static_cast<std::size_t>(ModuleIndex::Main);
    const auto& moduleBits = exl::util::impl::mem_layout::s_ModuleBitset;
    const auto& moduleInfos = exl::util::impl::mem_layout::s_ModuleInfos;
    if (!moduleBits[mainIndex]) {
        svcOutputDebugString(
            "[NativeHook] main module missing; Init observer not installed.\n",
            sizeof("[NativeHook] main module missing; Init observer not installed.\n") - 1);
        return false;
    }

    char buildIdText[65]{};
    FormatBuildId(FindGnuBuildId(moduleInfos[mainIndex]), buildIdText,
                  sizeof(buildIdText));
    if (std::strcmp(buildIdText, silkmodloader::game::kVerifiedMainBuildIdText) != 0) {
        svcOutputDebugString(
            "[NativeHook] Build ID not recognized; Init observer refused.\n",
            sizeof("[NativeHook] Build ID not recognized; Init observer refused.\n") - 1);
        return false;
    }

    Tk2dInitObserverHook::InstallAtOffset(
        silkmodloader::game::kVerifiedIl2CppOffsets.tk2dSpriteCollectionData_Init);
    ProbeLine line;
    line.Append("[NativeHook] installed original-preserving tk2d Init observer at ");
    line.AppendHex(moduleInfos[mainIndex].m_Total.m_Start +
                   silkmodloader::game::kVerifiedIl2CppOffsets
                       .tk2dSpriteCollectionData_Init);
    line.Append(" (main+0x5b58b20)\n");
    line.Flush();
    return true;
}
#endif

[[maybe_unused]] void EmitModuleProbe() {
    using exl::util::ModuleIndex;

    /* Snapshot the exlaunch table once. Do not call GetModuleInfo() here:
     * the probe must never turn a transient/missing module into an abort. */
    const auto moduleBits = exl::util::impl::mem_layout::s_ModuleBitset;
    const auto moduleInfos = exl::util::impl::mem_layout::s_ModuleInfos;

    ProbeLine header;
    header.Append("[SilkModLoader] Module and Build ID probe\n");
    header.Flush();

    for (int value = static_cast<int>(ModuleIndex::Start);
         value < static_cast<int>(ModuleIndex::End); ++value) {
        const auto index = static_cast<ModuleIndex>(value);
        if (!moduleBits[static_cast<std::size_t>(value)]) {
            continue;
        }

        const auto& module = moduleInfos[static_cast<std::size_t>(value)];
        const auto buildId = FindGnuBuildId(module);
        const auto* name = ModuleIndexName(index);

        ProbeLine line;
        line.Append("[Module] ");
        line.Append(name);
        line.Append(" base=");
        line.AppendHex(module.m_Total.m_Start);
        line.Append(" text=");
        line.AppendHex(module.m_Text.m_Start);
        line.Append(" size=");
        line.AppendHex(module.m_Text.m_Size);
        line.Append("\n");
        line.Flush();

        ProbeLine ranges;
        ranges.Append("[Module] ");
        ranges.Append(name);
        ranges.Append(" rodata=");
        ranges.AppendHex(module.m_Rodata.m_Start);
        ranges.Append(" size=");
        ranges.AppendHex(module.m_Rodata.m_Size);
        ranges.Append(" data=");
        ranges.AppendHex(module.m_Data.m_Start);
        ranges.Append(" size=");
        ranges.AppendHex(module.m_Data.m_Size);
        ranges.Append("\n");
        ranges.Flush();

        ProbeLine id;
        id.Append("[Module] ");
        id.Append(name);
        id.Append(" build-id=");
        id.AppendBuildId(buildId);
        id.Append("\n");
        id.Flush();
    }

    ProbeLine game;
    game.Append("[Game] Silksong Build ID: ");
    const auto mainValue = static_cast<std::size_t>(ModuleIndex::Main);
    if (moduleBits[mainValue]) {
        game.AppendBuildId(FindGnuBuildId(moduleInfos[mainValue]));
    } else {
        game.Append("<main-module-missing>");
    }
    game.Append("\n");
    game.Flush();

    if (moduleBits[mainValue]) {
        const auto mainBuildId = FindGnuBuildId(moduleInfos[mainValue]);
        char mainBuildIdText[65]{};
        FormatBuildId(mainBuildId, mainBuildIdText, sizeof(mainBuildIdText));
        const bool buildIdRecognized =
            std::strcmp(mainBuildIdText,
                        silkmodloader::game::kVerifiedMainBuildIdText) == 0;
        ProbeLine binding;
        binding.Append("[IL2CPP] Build ID ");
        if (buildIdRecognized) {
            binding.Append("recognized; read-only RVA table bound\n");
        } else {
            binding.Append("unknown; dangerous hooks refused actual=");
            binding.Append(mainBuildIdText[0] == '\0' ? "<not-found>" :
                           std::string_view(mainBuildIdText));
            binding.Append(" expected=");
            binding.Append(silkmodloader::game::kVerifiedMainBuildIdText);
            binding.Append("\n");
        }
        binding.Flush();

        if (buildIdRecognized) {
            const auto& main = moduleInfos[mainValue];
            const auto& offsets = silkmodloader::game::kVerifiedIl2CppOffsets;
            ProbeLine addresses;
            addresses.Append("[IL2CPP] main+InitMaterialIds=");
            addresses.AppendHex(main.m_Total.m_Start +
                                offsets.tk2dSpriteCollectionData_InitMaterialIds);
            addresses.Append(" LoadImage(byte[])=");
            addresses.AppendHex(main.m_Total.m_Start +
                                offsets.imageConversion_LoadImage_ByteArray);
            addresses.Append(" (no runtime patch installed; prelaunch patch may be present)\n");
            addresses.Flush();

            /* First external-hook phase: calculate and verify only. */
            silkmodloader::hook::EmitReadOnlyHookPlan(
                mainBuildIdText,
                silkmodloader::game::kVerifiedTk2dInitCallTarget);
        }
    }
}

} // namespace
#endif

extern "C" void exl_main(void*, void*) {
#ifdef SILKMODLOADER_NATIVE_HOOK_PROBE
    static constexpr char kProbeMessage[] =
        "[SilkModLoader]\n"
        "Loader started.\n"
        "[Probe] native tk2d Init observer; original function is preserved; no Unity object is modified.\n";
    svcOutputDebugString(kProbeMessage, sizeof(kProbeMessage) - 1);
    EmitModuleProbe();
    InstallNativeHookIfSupported();
#elif defined(SILKMODLOADER_MODULE_PROBE)
    static constexpr char kProbeMessage[] =
        "[SilkModLoader]\n"
        "Loader started.\n"
        "[Probe] module enumeration and Build ID only; SD logging and Unity hooks are disabled.\n";
    svcOutputDebugString(kProbeMessage, sizeof(kProbeMessage) - 1);
    /* Populate only the read-only exlaunch module table. Do not call exl_init:
     * that would also initialize the patcher, relocator, and version system. */
    exl::util::impl::InitMemLayout();
    EmitModuleProbe();
#elif defined(SILKMODLOADER_SVC_ONLY_BOOTSTRAP)
    static constexpr char kProbeMessage[] =
        "[SilkModLoader]\n"
        "Loader started.\n"
        "[Probe] svc-only bootstrap; SD logging and Unity hooks are disabled.\n";
    svcOutputDebugString(kProbeMessage, sizeof(kProbeMessage) - 1);
#else
#ifdef SILKMODLOADER_ENABLE_RUNTIME_SKIN
    /* Runtime-skin diagnostics use GDB's svcOutputDebugString capture. Do not
     * resolve the legacy MountSdCardForDebug path: Silksong's newer rtld
     * layout makes the old exlaunch ModuleObject lookup unsafe, and the skin
     * reader already uses the verified rom: filesystem path. */
    Logging.Log("[SilkModLoader] %.*s", static_cast<int>(kLoaderVersion.size()),
                kLoaderVersion.data());
    Logging.Log("[SilkModLoader] Loader started.");
    Logging.Log("[FileLog] disabled for runtime-skin build; use GDB SVC capture.");
#else
    const auto fileLog = Logging.GetLogger<silkmodloader::SdFileLogger>().Initialize();
    Logging.Log("[SilkModLoader] %.*s", static_cast<int>(kLoaderVersion.size()),
                kLoaderVersion.data());
    Logging.Log("[SilkModLoader] Loader started.");
    Logging.Log("[FileLog] path=%s status=%s mount=0x%08x open=0x%08x",
                silkmodloader::SdFileLogger::LogPath,
                fileLog.enabled ? "ENABLED" : "UNAVAILABLE",
                fileLog.mountResult, fileLog.openResult);
    if (fileLog.missingSymbol != nullptr) {
        Logging.Log("[FileLog] missing symbol: %s", fileLog.missingSymbol);
    }
#endif
    LogModules();

    const auto mainBuildId = FindGnuBuildId(exl::util::GetMainModuleInfo());
    char mainBuildIdText[65]{};
    FormatBuildId(mainBuildId, mainBuildIdText, sizeof(mainBuildIdText));
    Logging.Log("[Game] Silksong Build ID: %s",
                mainBuildIdText[0] == '\0' ? "UNKNOWN" : mainBuildIdText);

#ifdef SILKMODLOADER_ENABLE_RUNTIME_SKIN
    if (std::strcmp(mainBuildIdText,
                    silkmodloader::game::kVerifiedMainBuildIdText) == 0) {
        /* The external prelaunch BL reaches this module entry.  Resolve and
         * bind the original TK2D Init before the first callback; no runtime
         * instruction write is performed here. */
        silkmodloader::hook::EmitReadOnlyHookPlan(
            mainBuildIdText, silkmodloader::game::kVerifiedTk2dInitCallTarget);
        silkmodloader::hook::EmitReadOnlyMaterialHookPlan(
            mainBuildIdText,
            silkmodloader::game::kVerifiedMaterialSetMainTextureCallTarget);
        silkmodloader::hook::EmitReadOnlySpriteRendererHookPlan(
            mainBuildIdText,
            silkmodloader::game::kVerifiedSpriteRendererSetSpriteCallTarget);
        silkmodloader::hook::EmitReadOnlyUIImageHookPlan(
            mainBuildIdText,
            silkmodloader::game::kVerifiedUIImageSetSpriteCallTarget);
    } else {
        Logging.Log("[Hook] installation status: SKIPPED (unknown Build ID)");
    }
#else
    // The supported-build table remains disabled in the safe probe build.
    Logging.Log("[Hook] installation status: SKIPPED (runtime skin disabled)");
#endif
#endif
}

extern "C" NORETURN void exl_exception_entry() {
    EXL_ABORT("SilkModLoader exception handler called");
}
