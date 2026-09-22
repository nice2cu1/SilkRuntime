#pragma once

#include <cstdint>

namespace silkmodloader::hook {

enum class BranchType : std::uint8_t {
    B,
    BL,
};

enum class BranchError : std::uint8_t {
    None,
    NullAddress,
    Unaligned,
    OutOfRange,
};

struct BranchResult {
    bool valid;
    std::uint32_t instruction;
    std::int64_t delta;
    BranchError error;
};

constexpr bool CanDirectBranch(std::uintptr_t from, std::uintptr_t to) {
    if (from == 0 || to == 0) {
        return false;
    }

    const auto delta = static_cast<std::int64_t>(to) -
                       static_cast<std::int64_t>(from);
    return (delta & 0x3) == 0 &&
           delta >= -(1LL << 27) &&
           delta < (1LL << 27);
}

constexpr BranchResult EncodeBranch(
    std::uintptr_t from,
    std::uintptr_t to,
    BranchType type
) {
    if (from == 0 || to == 0) {
        return {false, 0, 0, BranchError::NullAddress};
    }
    if ((from & 0x3) != 0 || (to & 0x3) != 0) {
        return {false, 0, 0, BranchError::Unaligned};
    }

    const auto delta = static_cast<std::int64_t>(to) -
                       static_cast<std::int64_t>(from);
    if ((delta & 0x3) != 0 ||
        delta < -(1LL << 27) ||
        delta >= (1LL << 27)) {
        return {false, 0, delta, BranchError::OutOfRange};
    }

    const auto imm26 = delta >> 2;
    const auto opcode = type == BranchType::BL ? 0x94000000u : 0x14000000u;
    const auto instruction = opcode |
        (static_cast<std::uint32_t>(imm26) & 0x03ffffffu);
    return {true, instruction, delta, BranchError::None};
}

const char* BranchErrorName(BranchError error);
const char* BranchTypeName(BranchType type);

static_assert(EncodeBranch(0x1000, 0x1004, BranchType::B).instruction ==
              0x14000001u);
static_assert(EncodeBranch(0x1000, 0x1004, BranchType::BL).instruction ==
              0x94000001u);
static_assert(!EncodeBranch(0x1000, 0x1000 + (1LL << 27), BranchType::BL).valid);

} // namespace silkmodloader::hook
