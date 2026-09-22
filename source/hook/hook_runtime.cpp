#include "hook_runtime.hpp"

#include "tk2d_external_hook.hpp"
#include "material_external_hook.hpp"
#include "sprite_external_hook.hpp"

#include "lib.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

#include <lib/util/module_index.hpp>
#include <lib/util/sys/mem_layout.hpp>

namespace silkmodloader::hook {
namespace {

class LogLine {
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

    void AppendHex32(std::uint32_t value) {
        constexpr char kHex[] = "0123456789abcdef";
        Append("0x");
        for (int shift = 28; shift >= 0; shift -= 4) {
            const auto digit = static_cast<unsigned>((value >> shift) & 0xfU);
            Append(std::string_view(&kHex[digit], 1));
        }
    }

    void AppendSigned(std::int64_t value) {
        if (value < 0) {
            Append("-");
            /* Avoid signed overflow for the minimum value. */
            const auto magnitude = static_cast<std::uint64_t>(-(value + 1)) + 1;
            AppendUnsigned(magnitude);
            return;
        }
        AppendUnsigned(static_cast<std::uint64_t>(value));
    }

    void Flush() {
        svcOutputDebugString(m_Buffer, m_Size);
        m_Size = 0;
    }

private:
    void AppendUnsigned(std::uint64_t value) {
        char digits[32]{};
        std::size_t count = 0;
        do {
            digits[count++] = static_cast<char>('0' + (value % 10));
            value /= 10;
        } while (value != 0 && count < sizeof(digits));
        while (count != 0) {
            Append(std::string_view(&digits[--count], 1));
        }
    }

    /* The address/branch diagnostic line contains several 64-bit values and
     * was previously truncated exactly before the BranchType text. */
    char m_Buffer[512]{};
    std::size_t m_Size{};
};

struct ModuleBases {
    const exl::util::ModuleInfo* main{};
    const exl::util::ModuleInfo* loader{};
};

ModuleBases DiscoverBases() {
    ModuleBases result{};
    const auto bits = exl::util::impl::mem_layout::s_ModuleBitset;
    const auto& infos = exl::util::impl::mem_layout::s_ModuleInfos;
    const auto mainIndex = static_cast<std::size_t>(exl::util::ModuleIndex::Main);
    if (bits[mainIndex]) {
        result.main = &infos[mainIndex];
    }

    for (int value = static_cast<int>(exl::util::ModuleIndex::Start);
         value < static_cast<int>(exl::util::ModuleIndex::End); ++value) {
        const auto index = static_cast<exl::util::ModuleIndex>(value);
        if (!bits[static_cast<std::size_t>(value)] || index == exl::util::ModuleIndex::Main) {
            continue;
        }
        const auto& info = infos[static_cast<std::size_t>(value)];
        const auto name = info.GetModuleName();
        if (name.find("SilkModLoader") != std::string_view::npos ||
            name.find("subsdk9") != std::string_view::npos) {
            result.loader = &info;
            break;
        }
    }
    /* Newer RTLD tables may not expose a module path.  In this title the
     * injected subsdk9 occupies the first free subsdk slot (subsdk1); use the
     * explicit slot only as a fallback, never in preference to a named match.
     */
    if (result.loader == nullptr) {
        constexpr exl::util::ModuleIndex kFallbacks[] = {
            exl::util::ModuleIndex::Subsdk9,
            exl::util::ModuleIndex::Subsdk1,
        };
        for (const auto index : kFallbacks) {
            const auto slot = static_cast<std::size_t>(index);
            if (bits[slot] && &infos[slot] != result.main) {
                result.loader = &infos[slot];
                break;
            }
        }
    }
    return result;
}

void EmitError(std::string_view text) {
    LogLine line;
    line.Append("[NSHook][ERROR] ");
    line.Append(text);
    line.Append("\n");
    line.Flush();
}

} // namespace

namespace {

using OriginalBinder = void (*)(std::uintptr_t);

void EmitReadOnlyHookPlanImpl(const char* buildId, const HookTarget& target,
                              std::uintptr_t hookAddress,
                              std::string_view abi,
                              OriginalBinder bindOriginal) {
    const auto modules = DiscoverBases();
    if (modules.main == nullptr) {
        EmitError("main module missing; read-only hook plan refused");
        return;
    }
    if (modules.loader == nullptr) {
        EmitError("SilkModLoader module missing; hook address cannot be resolved");
        return;
    }

    const auto mainBase = modules.main->m_Total.m_Start;
    const auto loaderBase = modules.loader->m_Total.m_Start;
    const auto callSite = mainBase + target.callSiteOffset;
    const auto original = mainBase + target.originalFunctionOffset;
    const auto hookOffset = hookAddress - loaderBase;

    LogLine header;
    header.Append("[NSHook] Build ID: ");
    if (buildId == nullptr) {
        header.Append("<null>");
    } else {
        header.Append(std::string_view(buildId));
    }
    header.Append("\n");
    header.Append("[NSHook] main:    ");
    header.AppendHex(mainBase);
    header.Append("\n[NSHook] subsdk9: ");
    header.AppendHex(loaderBase);
    header.Append("\n");
    header.Flush();

    LogLine addresses;
    addresses.Append("[NSHook] Target: ");
    addresses.Append(target.name == nullptr ? "<unnamed>" : std::string_view(target.name));
    addresses.Append("\n[NSHook]   CallSite : ");
    addresses.AppendHex(callSite);
    addresses.Append("\n[NSHook]   Original : ");
    addresses.AppendHex(original);
    addresses.Append("\n[NSHook]   Hook     : ");
    addresses.AppendHex(hookAddress);
    addresses.Append("\n[NSHook]   TargetOffset: ");
    addresses.AppendHex(target.callSiteOffset);
    addresses.Append("\n[NSHook]   HookOffset: ");
    addresses.AppendHex(hookOffset);
    addresses.Append("\n[NSHook]   Delta    : ");
    addresses.AppendSigned(static_cast<std::int64_t>(hookAddress) -
                           static_cast<std::int64_t>(callSite));
    addresses.Append("\n[NSHook]   Branch   : ");
    addresses.Append(BranchTypeName(target.branchType));
    addresses.Append("\n[NSHook]   ABI      : ");
    addresses.Append(abi);
    addresses.Append("\n");
    addresses.Flush();

    const auto textStart = modules.main->m_Text.m_Start;
    const auto textEnd = textStart + modules.main->m_Text.m_Size;
    if (target.callSiteOffset == 0 || callSite < textStart || callSite + 4 > textEnd) {
        EmitError("call-site is outside main text or not bound");
        return;
    }

    const auto actualInstruction = *reinterpret_cast<const std::uint32_t*>(callSite);
    const auto branch = EncodeBranch(callSite, hookAddress, target.branchType);
    LogLine instruction;
    instruction.Append("[NSHook]   Expected : ");
    instruction.AppendHex32(target.expectedInstruction);
    instruction.Append("\n[NSHook]   Actual   : ");
    instruction.AppendHex32(actualInstruction);
    instruction.Append("\n");
    instruction.Flush();
    const bool originalInstruction = actualInstruction == target.expectedInstruction;
    const bool prelaunchInstruction = branch.valid &&
                                      actualInstruction == branch.instruction;
    if (!originalInstruction && !prelaunchInstruction) {
        EmitError("call-site opcode mismatch; patch plan refused");
        return;
    }

    LogLine result;
    result.Append("[NSHook]   DirectBranch: ");
    result.Append(branch.valid ? "YES" : "NO");
    result.Append("\n[NSHook]   Opcode   : ");
    if (branch.valid) {
        result.AppendHex32(branch.instruction);
    } else {
        result.Append("<invalid>");
    }
    result.Append("\n[NSHook]   Plan     : ");
    result.Append(prelaunchInstruction ?
                  "PRELAUNCH_PATCHED (no runtime instruction write)\n" :
                  "READ_ONLY (no instruction write)\n");
    result.Flush();
    if (!branch.valid) {
        LogLine error;
        error.Append("[NSHook][ERROR] branch encoding refused: ");
        error.Append(BranchErrorName(branch.error));
        error.Append("\n");
        error.Flush();
        return;
    }

    /* Bind the original only after all read-only safety checks pass.  No
     * instruction is written here; an external dmnt patch may use this
     * function later, while the current build remains a passive probe. */
    if (bindOriginal != nullptr) {
        bindOriginal(original);
    }
}

} // namespace

void EmitReadOnlyHookPlan(const char* buildId, const HookTarget& target) {
    EmitReadOnlyHookPlanImpl(
        buildId, target, GetTk2dExternalHookAddress(),
        "void(void*, MethodInfo*)", &BindTk2dOriginal);
}

void EmitReadOnlyMaterialHookPlan(const char* buildId,
                                  const HookTarget& target) {
    EmitReadOnlyHookPlanImpl(
        buildId, target, GetMaterialSetMainTextureExternalHookAddress(),
        "void(void*, Texture*, MethodInfo*)",
        &BindMaterialSetMainTextureOriginal);
}

void EmitReadOnlySpriteRendererHookPlan(const char* buildId,
                                        const HookTarget& target) {
    EmitReadOnlyHookPlanImpl(
        buildId, target, GetSpriteRendererSetSpriteExternalHookAddress(),
        "void(void*, Sprite*, MethodInfo*)",
        &BindSpriteRendererSetSpriteOriginal);
}

void EmitReadOnlyUIImageHookPlan(const char* buildId,
                                 const HookTarget& target) {
    EmitReadOnlyHookPlanImpl(
        buildId, target, GetUIImageSetSpriteExternalHookAddress(),
        "void(void*, Sprite*, MethodInfo*)", &BindUIImageSetSpriteOriginal);
}

} // namespace silkmodloader::hook
