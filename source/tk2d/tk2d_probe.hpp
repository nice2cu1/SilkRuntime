#pragma once

#include <cstddef>
#include <cstdint>

namespace silkmodloader::tk2d {

constexpr std::size_t kMaxObservedAtlases = 4;

struct CollectionSnapshot {
    char name[0x40]{};
    std::uint32_t materialCount{};
    std::uint32_t textureCount{};
    std::uint32_t materialPngTextureIdCount{};
    void* textures[kMaxObservedAtlases]{};
    std::int32_t materialPngTextureIds[kMaxObservedAtlases]{};
};

bool CaptureCollection(void* collection, CollectionSnapshot* output);
bool IsCollectionNamed(const CollectionSnapshot& collection,
                       const char* name);

} // namespace silkmodloader::tk2d
