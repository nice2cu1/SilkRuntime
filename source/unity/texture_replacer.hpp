#pragma once

#include <cstddef>
#include <cstdint>

namespace silkmodloader::unity {

struct ManagedSpanWrapper {
    void* begin{};
    std::int32_t length{};
};

struct TextureMetadata {
    std::uint32_t width{};
    std::uint32_t height{};
    std::int32_t format{};
    std::uintptr_t nativePointer{};
};

enum class TextureOperation : std::uint8_t {
    Applied,
    InvalidArgument,
    NotBound,
    NullUnityPointer,
    MetadataReadFailed,
    DimensionMismatch,
    UnsupportedFormat,
    LoadFailed,
};

class TextureReplacer final {
public:
    /* Offsets are used only after the caller has verified the exact Build ID. */
    bool BindVerifiedMain(std::uintptr_t mainBase);

    bool ReadMetadata(void* managedTexture, TextureMetadata* output) const;

    bool ReadName(void* managedTexture, char* output,
                  std::size_t capacity) const;

    void* GetSpriteTexture(void* sprite) const;

    TextureOperation ApplyPng(void* managedTexture, const void* pngData,
                              std::size_t pngSize, std::uint32_t expectedWidth,
                              std::uint32_t expectedHeight) const;

private:
    using TextureGetterFn = std::int32_t (*)(void*, const void*);
    using GetNameFn = void* (*)(void*, const void*);
    using LoadImageInjectedFn = bool (*)(std::uintptr_t, ManagedSpanWrapper*,
                                         bool, const void*);

    TextureGetterFn m_GetWidth{};
    TextureGetterFn m_GetHeight{};
    TextureGetterFn m_GetFormat{};
    GetNameFn m_GetName{};
    GetNameFn m_GetSpriteTexture{};
    LoadImageInjectedFn m_LoadImageInjected{};
};

const char* TextureOperationName(TextureOperation operation);

} // namespace silkmodloader::unity
