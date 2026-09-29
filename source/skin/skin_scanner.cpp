#include "skin_scanner.hpp"

#include <cstring>

#include "image/png_loader.hpp"

namespace silkmodloader::skin {
namespace {

constexpr std::size_t kAtlasMaximumSize = SdFileReader::kMaxFileSize;
constexpr std::size_t kPngHeaderSize = 24;
constexpr std::uint64_t kMaxPixels = 8192ULL * 8192ULL;
constexpr char kHexDigits[] = "0123456789ABCDEF";

std::uint32_t ReadBigEndian32(const std::uint8_t* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) |
           static_cast<std::uint32_t>(bytes[3]);
}

bool CopyString(char* destination, std::size_t capacity,
                std::string_view value) {
    if (destination == nullptr || capacity == 0 || value.size() + 1 > capacity) {
        return false;
    }
    std::memcpy(destination, value.data(), value.size());
    destination[value.size()] = '\0';
    return true;
}

bool Append(char* output, std::size_t capacity, std::size_t* cursor,
            std::string_view value) {
    if (output == nullptr || cursor == nullptr ||
        *cursor + value.size() + 1 > capacity) {
        return false;
    }
    std::memcpy(output + *cursor, value.data(), value.size());
    *cursor += value.size();
    output[*cursor] = '\0';
    return true;
}

bool AppendChar(char* output, std::size_t capacity, std::size_t* cursor,
                char value) {
    if (output == nullptr || cursor == nullptr || *cursor + 2 > capacity) {
        return false;
    }
    output[(*cursor)++] = value;
    output[*cursor] = '\0';
    return true;
}

SdReaderStatus ReadPngInfo(SdFileReader* reader, const char* path,
                           image::PngInfo* output, std::size_t* fileSize) {
    if (reader == nullptr || path == nullptr || output == nullptr) {
        return {SdReadError::NotInitialized, 0};
    }

    std::uint8_t header[kPngHeaderSize]{};
    const auto read = reader->ReadPrefix(path, header, sizeof(header),
                                         kAtlasMaximumSize, fileSize);
    if (read.error != SdReadError::None) {
        return read;
    }

    constexpr std::uint8_t kPngSignature[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
    };
    if (std::memcmp(header, kPngSignature, sizeof(kPngSignature)) != 0 ||
        std::memcmp(header + 12, "IHDR", 4) != 0) {
        return {SdReadError::ReadFailed, 0};
    }

    const auto width = ReadBigEndian32(header + 16);
    const auto height = ReadBigEndian32(header + 20);
    if (width == 0 || height == 0 ||
        static_cast<std::uint64_t>(width) * height > kMaxPixels) {
        return {SdReadError::ReadFailed, 0};
    }
    output->width = width;
    output->height = height;
    return {SdReadError::None, 0};
}

} // namespace

void SkinScanner::BindVerifiedMain(std::uintptr_t mainBase) {
    m_Reader.BindVerifiedMain(mainBase);
}

SdReaderStatus SkinScanner::FindSingleSkinRoot(const char* parentPath,
                                               char* outputPath,
                                               std::size_t outputCapacity) {
    return m_Reader.FindSingleDirectory(parentPath, outputPath, outputCapacity);
}

bool SkinScanner::JoinPath(char* output, std::size_t capacity,
                           const char* root, const char* suffix) {
    if (output == nullptr || capacity == 0 || root == nullptr || suffix == nullptr) {
        return false;
    }

    const std::size_t rootLength = std::strlen(root);
    const bool hasSeparator = rootLength != 0 &&
                              (root[rootLength - 1] == '/' ||
                               root[rootLength - 1] == '\\');
    const std::size_t suffixLength = std::strlen(suffix);
    const std::size_t total = rootLength + (hasSeparator ? 0 : 1) + suffixLength;
    if (total + 1 > capacity) {
        return false;
    }

    std::memcpy(output, root, rootLength);
    std::size_t offset = rootLength;
    if (!hasSeparator) {
        output[offset++] = '/';
    }
    std::memcpy(output + offset, suffix, suffixLength);
    output[total] = '\0';
    return true;
}

bool SkinScanner::EncodeResourceFilename(std::string_view resource,
                                         char* output, std::size_t capacity) {
    if (output == nullptr || capacity < 4 || resource.empty()) {
        return false;
    }

    std::size_t cursor = 0;
    if (!Append(output, capacity, &cursor, "r_")) {
        return false;
    }
    for (const auto rawByte : resource) {
        const auto byte = static_cast<std::uint8_t>(rawByte);
        const bool simple =
            (byte >= 'A' && byte <= 'Z') ||
            (byte >= 'a' && byte <= 'z') ||
            (byte >= '0' && byte <= '9') || byte == '-';
        if (simple) {
            if (!AppendChar(output, capacity, &cursor,
                            static_cast<char>(byte))) {
                return false;
            }
            continue;
        }
        if (!AppendChar(output, capacity, &cursor, '_') ||
            !AppendChar(output, capacity, &cursor,
                        kHexDigits[(byte >> 4) & 0x0f]) ||
            !AppendChar(output, capacity, &cursor,
                        kHexDigits[byte & 0x0f])) {
            return false;
        }
    }
    return Append(output, capacity, &cursor, ".png");
}

SdReaderStatus SkinScanner::ResolveCollection(const char* skinRoot,
                                              std::string_view collection,
                                              std::uint32_t atlasIndex,
                                              ReplacementTexture* output) {
    if (output == nullptr || collection.empty() ||
        collection.size() >= kCollectionNameCapacity || atlasIndex > 9 ||
        collection.find('/') != std::string_view::npos ||
        collection.find('\\') != std::string_view::npos) {
        return {SdReadError::ReadFailed, 0};
    }

    char suffix[kReplacementPathCapacity]{};
    std::size_t cursor = 0;
    if (!Append(suffix, sizeof(suffix), &cursor, collection) ||
        !Append(suffix, sizeof(suffix), &cursor, "/atlas") ||
        !AppendChar(suffix, sizeof(suffix), &cursor,
                    static_cast<char>('0' + atlasIndex)) ||
        !Append(suffix, sizeof(suffix), &cursor, ".png")) {
        return {SdReadError::FileTooLarge, 0};
    }

    ReplacementTexture candidate{};
    if (!CopyString(candidate.collection, sizeof(candidate.collection), collection)) {
        return {SdReadError::FileTooLarge, 0};
    }
    char resource[] = "atlas0";
    resource[5] = static_cast<char>('0' + atlasIndex);
    if (!CopyString(candidate.resource, sizeof(candidate.resource), resource) ||
        !JoinPath(candidate.path, sizeof(candidate.path), skinRoot, suffix)) {
        return {SdReadError::FileTooLarge, 0};
    }

    image::PngInfo pngInfo{};
    std::size_t fileSize = 0;
    const auto read = ReadPngInfo(&m_Reader, candidate.path, &pngInfo, &fileSize);
    if (read.error != SdReadError::None) {
        return read;
    }
    candidate.atlasIndex = atlasIndex;
    candidate.width = pngInfo.width;
    candidate.height = pngInfo.height;
    candidate.fileSize = fileSize;
    candidate.kind = ReplacementKind::Collection;
    *output = candidate;
    return {SdReadError::None, 0};
}

SdReaderStatus SkinScanner::ResolveNamed(const char* skinRoot,
                                         std::string_view directory,
                                         std::string_view resource,
                                         ReplacementKind kind,
                                         ReplacementTexture* output) {
    if (output == nullptr || directory.empty() || resource.empty() ||
        resource.size() >= kResourceNameCapacity) {
        return {SdReadError::ReadFailed, 0};
    }

    char filename[kReplacementPathCapacity]{};
    if (!EncodeResourceFilename(resource, filename, sizeof(filename))) {
        return {SdReadError::FileTooLarge, 0};
    }

    char suffix[kReplacementPathCapacity]{};
    std::size_t cursor = 0;
    if (!Append(suffix, sizeof(suffix), &cursor, directory) ||
        !AppendChar(suffix, sizeof(suffix), &cursor, '/') ||
        !Append(suffix, sizeof(suffix), &cursor, filename)) {
        return {SdReadError::FileTooLarge, 0};
    }

    ReplacementTexture candidate{};
    if (!CopyString(candidate.resource, sizeof(candidate.resource), resource) ||
        !JoinPath(candidate.path, sizeof(candidate.path), skinRoot, suffix)) {
        return {SdReadError::FileTooLarge, 0};
    }

    image::PngInfo pngInfo{};
    std::size_t fileSize = 0;
    const auto read = ReadPngInfo(&m_Reader, candidate.path, &pngInfo, &fileSize);
    if (read.error != SdReadError::None) {
        return read;
    }
    candidate.width = pngInfo.width;
    candidate.height = pngInfo.height;
    candidate.fileSize = fileSize;
    candidate.kind = kind;
    *output = candidate;
    return {SdReadError::None, 0};
}

SdReaderStatus SkinScanner::ResolveStandalone(const char* skinRoot,
                                              std::string_view textureName,
                                              ReplacementTexture* output) {
    return ResolveNamed(skinRoot, "standalone", textureName,
                        ReplacementKind::Standalone, output);
}

SdReaderStatus SkinScanner::ResolveSpriteTexture(const char* skinRoot,
                                                std::string_view textureName,
                                                ReplacementTexture* output) {
    return ResolveNamed(skinRoot, "sprite", textureName,
                        ReplacementKind::SpriteTexture, output);
}

SdReaderStatus SkinScanner::ResolveSpriteAlias(const char* skinRoot,
                                               std::string_view spriteName,
                                               ReplacementTexture* output) {
    if (output == nullptr || spriteName.empty() ||
        spriteName.size() >= kResourceNameCapacity) {
        return {SdReadError::ReadFailed, 0};
    }

    char filename[0x100]{};
    if (!EncodeResourceFilename(spriteName, filename, sizeof(filename))) {
        return {SdReadError::FileTooLarge, 0};
    }
    char* extension = std::strrchr(filename, '.');
    if (extension == nullptr || std::strcmp(extension, ".png") != 0) {
        return {SdReadError::ReadFailed, 0};
    }
    std::memcpy(extension, ".map", sizeof(".map"));

    char suffix[kReplacementPathCapacity]{};
    std::size_t cursor = 0;
    if (!Append(suffix, sizeof(suffix), &cursor, "sprite-alias/") ||
        !Append(suffix, sizeof(suffix), &cursor, filename)) {
        return {SdReadError::FileTooLarge, 0};
    }
    char path[kReplacementPathCapacity]{};
    if (!JoinPath(path, sizeof(path), skinRoot, suffix)) {
        return {SdReadError::FileTooLarge, 0};
    }

    FileBuffer alias{};
    const auto read = m_Reader.ReadAll(path, &alias, kResourceNameCapacity);
    if (read.error != SdReadError::None) {
        return read;
    }
    if (alias.size == 0 || alias.size >= kResourceNameCapacity ||
        std::memchr(alias.data, '\0', alias.size) != nullptr) {
        return {SdReadError::ReadFailed, 0};
    }

    std::string_view textureName(reinterpret_cast<const char*>(alias.data),
                                 alias.size);
    while (!textureName.empty() &&
           (textureName.back() == '\n' || textureName.back() == '\r' ||
            textureName.back() == ' ' || textureName.back() == '\t')) {
        textureName.remove_suffix(1);
    }
    while (!textureName.empty() &&
           (textureName.front() == ' ' || textureName.front() == '\t')) {
        textureName.remove_prefix(1);
    }
    if (textureName.empty()) {
        return {SdReadError::ReadFailed, 0};
    }
    return ResolveSpriteTexture(skinRoot, textureName, output);
}

SdReaderStatus SkinScanner::ReadReplacement(const ReplacementTexture& replacement,
                                            FileBuffer* output) {
    return m_Reader.ReadAll(replacement.path, output);
}

} // namespace silkmodloader::skin
