#pragma once

#include <cstddef>
#include <cstdint>

namespace silkmodloader::unity {

struct ManagedSpanWrapper {
    void* begin{};
    std::int32_t length{};
};

struct TextureIdentity {
    std::uintptr_t nativePointer{};
    std::int32_t instanceId{};
};

struct TextureMetadata {
    std::uint32_t width{};
    std::uint32_t height{};
    std::int32_t format{};
    std::uintptr_t nativePointer{};
    std::int32_t instanceId{};
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

    /* Does not call Texture2D-specific getters. Managed wrappers may change
     * while referring to the same native Unity object. */
    bool ReadIdentity(void* managedObject, TextureIdentity* output) const;

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
    TextureGetterFn m_GetInstanceId{};
    LoadImageInjectedFn m_LoadImageInjected{};
};

const char* TextureOperationName(TextureOperation operation);

} // namespace silkmodloader::unity
