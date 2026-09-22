#pragma once

#include "branch_encoder.hpp"

#include <cstdint>

namespace silkmodloader::hook {

struct HookTarget {
    const char* name;
    std::uintptr_t callSiteOffset;
    std::uintptr_t originalFunctionOffset;
    std::uint32_t expectedInstruction;
    BranchType branchType;
};

} // namespace silkmodloader::hook
