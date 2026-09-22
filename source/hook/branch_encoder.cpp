#include "branch_encoder.hpp"

namespace silkmodloader::hook {

const char* BranchErrorName(BranchError error) {
    switch (error) {
        case BranchError::None: return "NONE";
        case BranchError::NullAddress: return "NULL_ADDRESS";
        case BranchError::Unaligned: return "UNALIGNED";
        case BranchError::OutOfRange: return "OUT_OF_RANGE";
        default: return "UNKNOWN";
    }
}

const char* BranchTypeName(BranchType type) {
    return type == BranchType::BL ? "BL" : "B";
}

} // namespace silkmodloader::hook
