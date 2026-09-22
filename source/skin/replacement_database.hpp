#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace silkmodloader::skin {

constexpr std::size_t kReplacementPathCapacity = 0x200;
constexpr std::size_t kCollectionNameCapacity = 0x40;
constexpr std::size_t kResourceNameCapacity = 0x40;
constexpr std::size_t kMaxReplacementTextures = 128;

enum class ReplacementKind : std::uint8_t {
    Collection,
    Standalone,
    SpriteTexture,
};

struct ReplacementTexture {
    char collection[kCollectionNameCapacity]{};
    char resource[kResourceNameCapacity]{};
    char path[kReplacementPathCapacity]{};
    std::uint32_t atlasIndex{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::size_t fileSize{};
    ReplacementKind kind{ReplacementKind::Collection};
};

class ReplacementDatabase final {
public:
    void Clear();

    bool Add(std::string_view collection, std::string_view resource,
             std::string_view path, std::uint32_t atlasIndex,
             std::uint32_t width, std::uint32_t height, std::size_t fileSize);

    bool AddStandalone(std::string_view textureName, std::string_view path,
                       std::uint32_t width, std::uint32_t height,
                       std::size_t fileSize);

    bool AddSpriteTexture(std::string_view textureName, std::string_view path,
                          std::uint32_t width, std::uint32_t height,
                          std::size_t fileSize);

    const ReplacementTexture* Find(std::string_view collection,
                                   std::uint32_t atlasIndex) const;

    const ReplacementTexture* FindStandalone(std::string_view textureName) const;

    const ReplacementTexture* FindSpriteTexture(std::string_view textureName) const;

    std::size_t CountForCollection(std::string_view collection) const;

    std::size_t Count() const { return m_Count; }

private:
    static bool CopyString(char* destination, std::size_t capacity,
                           std::string_view value);

    ReplacementTexture m_Textures[kMaxReplacementTextures]{};
    std::size_t m_Count{};
};

} // namespace silkmodloader::skin
