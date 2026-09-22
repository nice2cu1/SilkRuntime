#include "tk2d_probe.hpp"

#include <cstring>

namespace silkmodloader::tk2d {
namespace {

constexpr std::uintptr_t kObjectFieldsOffset = 0x10;
constexpr std::uintptr_t kCollectionNameOffset = 0x88;
constexpr std::uintptr_t kTexturesOffset = 0x60;
constexpr std::uintptr_t kMaterialPngTextureIdOffset = 0x70;

constexpr std::uintptr_t kArrayLengthOffset = 0x18;
constexpr std::uintptr_t kArrayDataOffset = 0x20;

std::uintptr_t ReadPointer(std::uintptr_t address) {
    return *reinterpret_cast<const std::uintptr_t*>(address);
}

std::uint32_t ReadArrayLength(std::uintptr_t array) {
    return array == 0 ? 0 : *reinterpret_cast<const std::uint32_t*>(
        array + kArrayLengthOffset);
}

bool ReadManagedAsciiString(std::uintptr_t object, char* output,
                            std::size_t capacity) {
    if (object == 0 || output == nullptr || capacity < 2) {
        return false;
    }

    const auto length = *reinterpret_cast<const std::int32_t*>(object + 0x10);
    if (length < 0 || static_cast<std::size_t>(length) + 1 > capacity ||
        length > 0x100) {
        return false;
    }

    const auto* chars = reinterpret_cast<const std::uint16_t*>(object + 0x14);
    for (std::int32_t index = 0; index < length; ++index) {
        if (chars[index] > 0x7f) {
            return false;
        }
        output[index] = static_cast<char>(chars[index]);
    }
    output[length] = '\0';
    return true;
}

} // namespace

bool CaptureCollection(void* collection, CollectionSnapshot* output) {
    if (collection == nullptr || output == nullptr) {
        return false;
    }
    *output = {};

    const auto base = reinterpret_cast<std::uintptr_t>(collection);
    const auto name = ReadPointer(base + kCollectionNameOffset);
    if (!ReadManagedAsciiString(name, output->name, sizeof(output->name))) {
        return false;
    }

    const auto textures = ReadPointer(base + kTexturesOffset);
    const auto textureCount = ReadArrayLength(textures);
    const auto ids = ReadPointer(base + kMaterialPngTextureIdOffset);
    const auto idCount = ReadArrayLength(ids);
    output->textureCount = textureCount;
    output->materialPngTextureIdCount = idCount;
    output->materialCount = idCount;

    const auto observedTextureCount = textureCount < kMaxObservedAtlases
                                          ? textureCount
                                          : kMaxObservedAtlases;
    for (std::uint32_t index = 0; index < observedTextureCount; ++index) {
        output->textures[index] = reinterpret_cast<void*>(ReadPointer(
            textures + kArrayDataOffset + index * sizeof(std::uintptr_t)));
    }

    const auto observedIdCount = idCount < kMaxObservedAtlases
                                     ? idCount
                                     : kMaxObservedAtlases;
    for (std::uint32_t index = 0; index < observedIdCount; ++index) {
        output->materialPngTextureIds[index] = *reinterpret_cast<const std::int32_t*>(
            ids + kArrayDataOffset + index * sizeof(std::int32_t));
    }
    return true;
}

bool IsCollectionNamed(const CollectionSnapshot& collection, const char* name) {
    return name != nullptr && std::strcmp(collection.name, name) == 0;
}

} // namespace silkmodloader::tk2d
