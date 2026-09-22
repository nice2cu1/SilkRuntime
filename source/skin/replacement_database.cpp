#include "replacement_database.hpp"

#include <cstring>

namespace silkmodloader::skin {

void ReplacementDatabase::Clear() {
    m_Count = 0;
    for (auto& texture : m_Textures) {
        texture = {};
    }
}

bool ReplacementDatabase::CopyString(char* destination, std::size_t capacity,
                                     std::string_view value) {
    if (destination == nullptr || capacity == 0 || value.size() + 1 > capacity) {
        return false;
    }

    std::memcpy(destination, value.data(), value.size());
    destination[value.size()] = '\0';
    return true;
}

bool ReplacementDatabase::Add(std::string_view collection,
                               std::string_view resource,
                               std::string_view path,
                               std::uint32_t atlasIndex,
                               std::uint32_t width,
                               std::uint32_t height,
                               std::size_t fileSize) {
    if (m_Count >= kMaxReplacementTextures) {
        return false;
    }

    ReplacementTexture candidate{};
    if (!CopyString(candidate.collection, sizeof(candidate.collection), collection) ||
        !CopyString(candidate.resource, sizeof(candidate.resource), resource) ||
        !CopyString(candidate.path, sizeof(candidate.path), path)) {
        return false;
    }

    candidate.atlasIndex = atlasIndex;
    candidate.width = width;
    candidate.height = height;
    candidate.fileSize = fileSize;
    candidate.kind = ReplacementKind::Collection;
    m_Textures[m_Count++] = candidate;
    return true;
}

bool ReplacementDatabase::AddStandalone(std::string_view textureName,
                                        std::string_view path,
                                        std::uint32_t width,
                                        std::uint32_t height,
                                        std::size_t fileSize) {
    const auto index = m_Count;
    if (!Add({}, textureName, path, 0, width, height, fileSize)) {
        return false;
    }
    m_Textures[index].kind = ReplacementKind::Standalone;
    return true;
}

bool ReplacementDatabase::AddSpriteTexture(std::string_view textureName,
                                           std::string_view path,
                                           std::uint32_t width,
                                           std::uint32_t height,
                                           std::size_t fileSize) {
    if (m_Count >= kMaxReplacementTextures) {
        return false;
    }

    ReplacementTexture candidate{};
    if (!CopyString(candidate.resource, sizeof(candidate.resource), textureName) ||
        !CopyString(candidate.path, sizeof(candidate.path), path)) {
        return false;
    }
    candidate.width = width;
    candidate.height = height;
    candidate.fileSize = fileSize;
    candidate.kind = ReplacementKind::SpriteTexture;
    m_Textures[m_Count++] = candidate;
    return true;
}

const ReplacementTexture* ReplacementDatabase::Find(
    std::string_view collection, std::uint32_t atlasIndex) const {
    for (std::size_t index = 0; index < m_Count; ++index) {
        const auto& texture = m_Textures[index];
        if (texture.kind == ReplacementKind::Collection &&
            texture.atlasIndex == atlasIndex &&
            std::string_view(texture.collection) == collection) {
            return &texture;
        }
    }
    return nullptr;
}

const ReplacementTexture* ReplacementDatabase::FindStandalone(
    std::string_view textureName) const {
    for (std::size_t index = 0; index < m_Count; ++index) {
        const auto& texture = m_Textures[index];
        if (texture.kind == ReplacementKind::Standalone &&
            texture.collection[0] == '\0' &&
            std::string_view(texture.resource) == textureName) {
            return &texture;
        }
    }
    return nullptr;
}

const ReplacementTexture* ReplacementDatabase::FindSpriteTexture(
    std::string_view textureName) const {
    for (std::size_t index = 0; index < m_Count; ++index) {
        const auto& texture = m_Textures[index];
        if (texture.kind == ReplacementKind::SpriteTexture &&
            std::string_view(texture.resource) == textureName) {
            return &texture;
        }
    }
    return nullptr;
}

std::size_t ReplacementDatabase::CountForCollection(
    std::string_view collection) const {
    std::size_t count = 0;
    for (std::size_t index = 0; index < m_Count; ++index) {
        if (m_Textures[index].kind == ReplacementKind::Collection &&
            std::string_view(m_Textures[index].collection) == collection) {
            ++count;
        }
    }
    return count;
}

} // namespace silkmodloader::skin
