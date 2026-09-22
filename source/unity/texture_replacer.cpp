#include "texture_replacer.hpp"

#include "program/offsets.hpp"

#include <cstdint>

namespace silkmodloader::unity {

bool TextureReplacer::BindVerifiedMain(std::uintptr_t mainBase) {
    if (mainBase == 0) {
        return false;
    }

    m_GetWidth = reinterpret_cast<TextureGetterFn>(
        mainBase + silkmodloader::game::kVerifiedIl2CppOffsets.texture_get_width);
    m_GetHeight = reinterpret_cast<TextureGetterFn>(
        mainBase + silkmodloader::game::kVerifiedIl2CppOffsets.texture_get_height);
    m_GetFormat = reinterpret_cast<TextureGetterFn>(
        mainBase + silkmodloader::game::kVerifiedIl2CppOffsets.texture2d_get_format);
    m_GetName = reinterpret_cast<GetNameFn>(
        mainBase + silkmodloader::game::kVerifiedIl2CppOffsets.object_get_name);
    m_GetSpriteTexture = reinterpret_cast<GetNameFn>(
        mainBase + silkmodloader::game::kVerifiedIl2CppOffsets.sprite_get_texture);
    m_LoadImageInjected = reinterpret_cast<LoadImageInjectedFn>(
        mainBase + silkmodloader::game::kVerifiedIl2CppOffsets
                       .imageConversion_LoadImage_Injected);
    return m_GetWidth != nullptr && m_GetHeight != nullptr &&
           m_GetFormat != nullptr && m_GetName != nullptr &&
           m_GetSpriteTexture != nullptr &&
           m_LoadImageInjected != nullptr;
}

bool TextureReplacer::ReadMetadata(void* managedTexture,
                                   TextureMetadata* output) const {
    if (managedTexture == nullptr || output == nullptr || m_GetWidth == nullptr ||
        m_GetHeight == nullptr || m_GetFormat == nullptr) {
        return false;
    }

    const auto nativePointer = *reinterpret_cast<const std::uintptr_t*>(
        reinterpret_cast<std::uintptr_t>(managedTexture) + 0x10);
    if (nativePointer == 0) {
        return false;
    }

    const auto width = m_GetWidth(managedTexture, nullptr);
    const auto height = m_GetHeight(managedTexture, nullptr);
    if (width <= 0 || height <= 0) {
        return false;
    }
    output->width = static_cast<std::uint32_t>(width);
    output->height = static_cast<std::uint32_t>(height);
    output->format = m_GetFormat(managedTexture, nullptr);
    output->nativePointer = nativePointer;
    return true;
}

bool TextureReplacer::ReadName(void* managedTexture, char* output,
                               std::size_t capacity) const {
    if (managedTexture == nullptr || output == nullptr || capacity < 2 ||
        m_GetName == nullptr) {
        return false;
    }

    output[0] = '\0';
    const auto stringObject = m_GetName(managedTexture, nullptr);
    if (stringObject == nullptr) {
        return false;
    }

    /* IL2CPP System.String stores its UTF-16 length at +0x10 and characters
     * at +0x14. All verified replacement keys are ASCII; refusing other
     * code points keeps the name comparison deterministic. */
    const auto length = *reinterpret_cast<const std::int32_t*>(
        reinterpret_cast<std::uintptr_t>(stringObject) + 0x10);
    if (length <= 0 || static_cast<std::size_t>(length) + 1 > capacity) {
        return false;
    }
    const auto* characters = reinterpret_cast<const std::uint16_t*>(
        reinterpret_cast<std::uintptr_t>(stringObject) + 0x14);
    for (std::int32_t index = 0; index < length; ++index) {
        const auto character = characters[index];
        if (character > 0x7f) {
            return false;
        }
        output[index] = static_cast<char>(character);
    }
    output[length] = '\0';
    return true;
}

void* TextureReplacer::GetSpriteTexture(void* sprite) const {
    if (sprite == nullptr || m_GetSpriteTexture == nullptr) {
        return nullptr;
    }
    return m_GetSpriteTexture(sprite, nullptr);
}

TextureOperation TextureReplacer::ApplyPng(
    void* managedTexture, const void* pngData, std::size_t pngSize,
    std::uint32_t expectedWidth, std::uint32_t expectedHeight) const {
    if (managedTexture == nullptr || pngData == nullptr || pngSize == 0 ||
        pngSize > 0x7fffffffU) {
        return TextureOperation::InvalidArgument;
    }
    if (m_LoadImageInjected == nullptr) {
        return TextureOperation::NotBound;
    }

    TextureMetadata metadata{};
    if (!ReadMetadata(managedTexture, &metadata)) {
        return TextureOperation::MetadataReadFailed;
    }
    if (metadata.width != expectedWidth || metadata.height != expectedHeight) {
        return TextureOperation::DimensionMismatch;
    }

    /* These are the only target formats present in the evidence-backed
     * package inputs: RGBA32 (4), DXT5/BC3 (12), ASTC 4x4 (48), and ASTC 6x6
     * (50). LoadImage is allowed to recreate the target texture from PNG, but
     * an unknown target format is refused before any write. */
    if (metadata.format != 4 && metadata.format != 12 &&
        metadata.format != 48 && metadata.format != 50) {
        return TextureOperation::UnsupportedFormat;
    }

    ManagedSpanWrapper span{
        .begin = const_cast<void*>(pngData),
        .length = static_cast<std::int32_t>(pngSize),
    };
    const bool loaded = m_LoadImageInjected(
        metadata.nativePointer, &span, false, nullptr);
    return loaded ? TextureOperation::Applied : TextureOperation::LoadFailed;
}

const char* TextureOperationName(TextureOperation operation) {
    switch (operation) {
        case TextureOperation::Applied: return "applied";
        case TextureOperation::InvalidArgument: return "invalid-argument";
        case TextureOperation::NotBound: return "not-bound";
        case TextureOperation::NullUnityPointer: return "null-unity-pointer";
        case TextureOperation::MetadataReadFailed: return "metadata-read-failed";
        case TextureOperation::DimensionMismatch: return "dimension-mismatch";
        case TextureOperation::UnsupportedFormat: return "unsupported-format";
        case TextureOperation::LoadFailed: return "load-failed";
        default: return "unknown";
    }
}

} // namespace silkmodloader::unity
