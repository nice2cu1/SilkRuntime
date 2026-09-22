/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Derived from exlaunch f9f4b0dd07b68f97958cb9c79228bbca22ca80d5.
 * Adapt module-name metadata; incorporates dt-12345's exlaunch PR #31 changes.
 * See THIRD_PARTY_NOTICES.md and licenses/source-inventory.json.
 */
#pragma once

#include <common.hpp>
#include <string_view>
#include "mod0.hpp"

#include "range.hpp"

namespace exl::util {
    struct ModuleInfo {
        static constexpr size_t s_ModulePathLengthMax = 0x200;

        Range m_Total;
        Range m_Text;
        Range m_Rodata;
        Range m_Data;
        const Mod0* m_Mod;

        std::string_view GetModulePath() const {
            const auto version = *reinterpret_cast<std::uint32_t*>(m_Rodata.m_Start);
            if(version == 0) {
                struct {
                    std::uint32_t m_Version;
                    std::uint32_t m_Length;
                    char m_Data[s_ModulePathLengthMax];
                }* module;
                auto ptr = reinterpret_cast<decltype(module)>(m_Rodata.m_Start);
                return { ptr->m_Data, ptr->m_Length };
            } else if(version == 1) {
                struct {
                    std::uint32_t m_Version;
                    std::uint32_t m_Mod0Offset;
                    std::uint32_t m_UnkOffset;
                    std::uint32_t m_Unk;
                    std::uint32_t m_Length;
                    char m_Data[s_ModulePathLengthMax];
                }* module;
                auto ptr = reinterpret_cast<decltype(module)>(m_Rodata.m_Start);
                return { ptr->m_Data, ptr->m_Length };
            }
            return "";
        }

        std::string_view GetModuleName() const {
            auto path = GetModulePath();
            if(path.empty())
                return path;

            /* Try to find the string after the last slash. */
            auto pos = path.find_last_of("/\\");
            if(pos == std::string_view::npos)
                pos = 0;
            else
                pos++;

            /* If it goes out of bounds somehow, just return the path. */
            if(path.size() < pos)
                return path;

            return path.substr(pos);
        }
    };
}
