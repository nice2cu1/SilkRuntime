#include "skin_loader.hpp"

#include "image/png_loader.hpp"
#include "replacement_database.hpp"
#include "skin_scanner.hpp"
#include "tk2d/tk2d_probe.hpp"
#include "unity/texture_replacer.hpp"

#include "lib.hpp"
#include "program/offsets.hpp"

#include <cstring>
#include <string_view>

namespace silkmodloader::skin {
namespace {

#ifdef SILKMODLOADER_ENABLE_RUNTIME_SKIN

constexpr char kSkinRootParent[] = "rom:/SilkModLoader/Mods/Skin";

SkinScanner g_Scanner;
unity::TextureReplacer g_TextureReplacer;
bool g_TextureBindingComplete{};
bool g_SkinAvailable{};
char g_SkinRoot[kReplacementPathCapacity]{};
bool g_ApplyInProgress{};

struct AppliedTextureRecord {
    char collection[kCollectionNameCapacity]{};
    std::uint32_t atlasIndex{};
    std::uintptr_t textureAddress{};
};

AppliedTextureRecord g_AppliedTextures[kMaxReplacementTextures]{};

struct AppliedStandaloneRecord {
    char name[kResourceNameCapacity]{};
    std::uintptr_t textureAddress{};
};

AppliedStandaloneRecord g_AppliedStandalone[kMaxReplacementTextures]{};

struct AppliedSpriteRecord {
    char name[kResourceNameCapacity]{};
    std::uintptr_t textureAddress{};
};

AppliedSpriteRecord g_AppliedSprites[kMaxReplacementTextures]{};

bool HexDigit(char value, std::uint8_t* output) {
    if (output == nullptr) {
        return false;
    }
    if (value >= '0' && value <= '9') {
        *output = static_cast<std::uint8_t>(value - '0');
        return true;
    }
    if (value >= 'a' && value <= 'f') {
        *output = static_cast<std::uint8_t>(value - 'a' + 10);
        return true;
    }
    if (value >= 'A' && value <= 'F') {
        *output = static_cast<std::uint8_t>(value - 'A' + 10);
        return true;
    }
    return false;
}

bool MainBuildIdMatches(const exl::util::ModuleInfo& module) {
    constexpr std::uint32_t kGnuBuildIdNote = 3;
    constexpr std::size_t kNoteHeaderSize = 12;
    constexpr char kExpected[] =
        "fc9ea4ccc955d5799f37752b2d730b31";

    const auto start = module.m_Rodata.m_Start;
    const auto size = module.m_Rodata.m_Size;
    for (std::size_t offset = 0; offset + kNoteHeaderSize <= size; offset += 4) {
        const auto* note = reinterpret_cast<const std::uint32_t*>(start + offset);
        const std::uint32_t nameSize = note[0];
        const std::uint32_t descriptionSize = note[1];
        if (nameSize != 4 || descriptionSize != 16 || note[2] != kGnuBuildIdNote) {
            continue;
        }

        const std::size_t alignedNameSize = (nameSize + 3U) & ~3U;
        const std::size_t totalSize = kNoteHeaderSize + alignedNameSize + descriptionSize;
        if (offset + totalSize > size ||
            std::memcmp(reinterpret_cast<const char*>(start + offset + kNoteHeaderSize),
                        "GNU", 4) != 0) {
            continue;
        }

        const auto* bytes = reinterpret_cast<const std::uint8_t*>(
            start + offset + kNoteHeaderSize + alignedNameSize);
        bool matches = true;
        for (std::size_t index = 0; index < descriptionSize; ++index) {
            std::uint8_t high{};
            std::uint8_t low{};
            if (!HexDigit(kExpected[index * 2], &high) ||
                !HexDigit(kExpected[index * 2 + 1], &low) ||
                bytes[index] != static_cast<std::uint8_t>((high << 4) | low)) {
                matches = false;
                break;
            }
        }
        return matches;
    }
    return false;
}

std::uintptr_t VerifiedMainBase() {
    using exl::util::ModuleIndex;
    const auto mainIndex = static_cast<std::size_t>(ModuleIndex::Main);
    const auto bits = exl::util::impl::mem_layout::s_ModuleBitset;
    if (!bits[mainIndex]) {
        return 0;
    }
    const auto& module = exl::util::impl::mem_layout::s_ModuleInfos[mainIndex];
    return MainBuildIdMatches(module) ? module.m_Total.m_Start : 0;
}

bool EnsureRuntimeState() {
    if (g_TextureBindingComplete) {
        return g_SkinAvailable;
    }

    const auto mainBase = VerifiedMainBase();
    if (mainBase == 0 || !g_TextureReplacer.BindVerifiedMain(mainBase)) {
        Logging.Log("[Skin] main Build ID not verified; Unity calls refused");
        return false;
    }
    g_Scanner.BindVerifiedMain(mainBase);

    const auto skinRoot = g_Scanner.FindSingleSkinRoot(
        kSkinRootParent, g_SkinRoot, sizeof(g_SkinRoot));
    g_TextureBindingComplete = true;
    if (skinRoot.error != SdReadError::None) {
        Logging.Log("[Skin] skin root discovery failed error=%s result=0x%08x",
                    SdReadErrorName(skinRoot.error), skinRoot.result);
        return false;
    }
    g_SkinAvailable = true;
    Logging.Log("[Skin] Unity Texture2D and RomFS reader bindings accepted "
                "for verified Build ID; skin root=%s", g_SkinRoot);
    return true;
}

bool WasApplied(std::string_view collection, std::uint32_t atlasIndex,
                std::uintptr_t textureAddress) {
    for (const auto& record : g_AppliedTextures) {
        if (record.textureAddress == textureAddress &&
            record.atlasIndex == atlasIndex &&
            std::string_view(record.collection) == collection) {
            return true;
        }
    }
    return false;
}

void MarkApplied(std::string_view collection, std::uint32_t atlasIndex,
                 std::uintptr_t textureAddress) {
    AppliedTextureRecord* target = nullptr;
    for (auto& record : g_AppliedTextures) {
        if (std::string_view(record.collection) == collection &&
            record.atlasIndex == atlasIndex) {
            target = &record;
            break;
        }
        if (target == nullptr && record.collection[0] == '\0') {
            target = &record;
        }
    }
    if (target == nullptr || collection.size() + 1 > sizeof(target->collection)) {
        return;
    }
    std::memcpy(target->collection, collection.data(), collection.size());
    target->collection[collection.size()] = '\0';
    target->atlasIndex = atlasIndex;
    target->textureAddress = textureAddress;
}

bool WasStandaloneApplied(std::string_view name,
                          std::uintptr_t textureAddress) {
    for (const auto& record : g_AppliedStandalone) {
        if (record.textureAddress == textureAddress &&
            std::string_view(record.name) == name) {
            return true;
        }
    }
    return false;
}

void MarkStandaloneApplied(std::string_view name,
                           std::uintptr_t textureAddress) {
    AppliedStandaloneRecord* target = nullptr;
    for (auto& record : g_AppliedStandalone) {
        if (std::string_view(record.name) == name) {
            target = &record;
            break;
        }
        if (target == nullptr && record.name[0] == '\0') {
            target = &record;
        }
    }
    if (target == nullptr || name.size() + 1 > sizeof(target->name)) {
        return;
    }
    std::memcpy(target->name, name.data(), name.size());
    target->name[name.size()] = '\0';
    target->textureAddress = textureAddress;
}

bool WasSpriteApplied(std::string_view name, std::uintptr_t textureAddress) {
    for (const auto& record : g_AppliedSprites) {
        if (record.textureAddress == textureAddress &&
            std::string_view(record.name) == name) {
            return true;
        }
    }
    return false;
}

void MarkSpriteApplied(std::string_view name, std::uintptr_t textureAddress) {
    AppliedSpriteRecord* target = nullptr;
    for (auto& record : g_AppliedSprites) {
        if (std::string_view(record.name) == name) {
            target = &record;
            break;
        }
        if (target == nullptr && record.name[0] == '\0') {
            target = &record;
        }
    }
    if (target == nullptr || name.size() + 1 > sizeof(target->name)) {
        return;
    }
    std::memcpy(target->name, name.data(), name.size());
    target->name[name.size()] = '\0';
    target->textureAddress = textureAddress;
}

void LogSnapshot(const tk2d::CollectionSnapshot& snapshot,
                 std::size_t replacementCount) {
    Logging.Log("[Skin] collection=%s materials=%u textures=%u ids=%u replacements=%u",
                snapshot.name, snapshot.materialCount, snapshot.textureCount,
                snapshot.materialPngTextureIdCount,
                static_cast<unsigned>(replacementCount));
    const auto observedCount = snapshot.textureCount < tk2d::kMaxObservedAtlases
                                   ? snapshot.textureCount
                                   : tk2d::kMaxObservedAtlases;
    for (std::uint32_t index = 0; index < observedCount; ++index) {
        Logging.Log("[Skin] atlas%u texture=%016lx materialPngTextureId=%d",
                    index, reinterpret_cast<std::uintptr_t>(snapshot.textures[index]),
                    index < snapshot.materialPngTextureIdCount
                        ? snapshot.materialPngTextureIds[index]
                        : -1);
    }
}

#endif

} // namespace

void OnCollectionInitialized(void* collection) {
#ifndef SILKMODLOADER_ENABLE_RUNTIME_SKIN
    EXL_UNUSED(collection);
    return;
#else
    if (g_ApplyInProgress) {
        return;
    }

    tk2d::CollectionSnapshot snapshot{};
    if (!tk2d::CaptureCollection(collection, &snapshot)) {
        return;
    }
    if (!EnsureRuntimeState()) {
        return;
    }

    const auto observedCount = snapshot.textureCount < tk2d::kMaxObservedAtlases
                                   ? snapshot.textureCount
                                   : tk2d::kMaxObservedAtlases;
    std::size_t replacementCount = 0;
    bool needsApply = false;
    for (std::uint32_t index = 0; index < observedCount; ++index) {
        if (snapshot.textures[index] == nullptr) {
            continue;
        }
        ReplacementTexture replacement{};
        const auto resolved = g_Scanner.ResolveCollection(
            g_SkinRoot, std::string_view(snapshot.name), index, &replacement);
        if (resolved.error != SdReadError::None) {
            continue;
        }
        ++replacementCount;
        const auto textureAddress = reinterpret_cast<std::uintptr_t>(
            snapshot.textures[index]);
        if (!WasApplied(snapshot.name, index, textureAddress)) {
            needsApply = true;
        }
    }
    if (replacementCount == 0 || !needsApply) {
        return;
    }

    LogSnapshot(snapshot, replacementCount);

    for (std::uint32_t index = 0; index < observedCount; ++index) {
        ReplacementTexture replacement{};
        const auto resolved = g_Scanner.ResolveCollection(
            g_SkinRoot, std::string_view(snapshot.name), index, &replacement);
        if (resolved.error != SdReadError::None) {
            continue;
        }

        const auto textureAddress = reinterpret_cast<std::uintptr_t>(
            snapshot.textures[index]);
        if (textureAddress == 0) {
            Logging.Log("[Skin] %s atlas%u rejected: texture pointer is null",
                        snapshot.name, index);
            continue;
        }
        if (WasApplied(snapshot.name, index, textureAddress)) {
            continue;
        }

        unity::TextureMetadata metadata{};
        if (!g_TextureReplacer.ReadMetadata(snapshot.textures[index], &metadata)) {
            Logging.Log("[Skin] %s atlas%u metadata read failed",
                        snapshot.name, index);
            continue;
        }
        Logging.Log("[Skin] atlas%u target=%ux%u format=%d native=%016lx",
                    index, metadata.width, metadata.height, metadata.format,
                    metadata.nativePointer);

        FileBuffer png{};
        const auto read = g_Scanner.ReadReplacement(replacement, &png);
        if (read.error != SdReadError::None) {
            Logging.Log("[Skin] %s atlas%u read failed error=%s result=0x%08x",
                        snapshot.name, index, SdReadErrorName(read.error),
                        read.result);
            continue;
        }

        image::PngInfo pngInfo{};
        if (!image::InspectPng(png.data, png.size, &pngInfo)) {
            Logging.Log("[Skin] %s atlas%u PNG validation failed",
                        snapshot.name, index);
            continue;
        }
        if (pngInfo.width != metadata.width || pngInfo.height != metadata.height) {
            Logging.Log("[Skin] %s atlas%u dimension mismatch png=%ux%u target=%ux%u",
                        snapshot.name, index, pngInfo.width, pngInfo.height,
                        metadata.width, metadata.height);
            continue;
        }

        g_ApplyInProgress = true;
        const auto operation = g_TextureReplacer.ApplyPng(
            snapshot.textures[index], png.data, png.size,
            replacement.width, replacement.height);
        g_ApplyInProgress = false;
        Logging.Log("[Skin] %s atlas%u operation=%s", snapshot.name, index,
                    unity::TextureOperationName(operation));
        if (operation == unity::TextureOperation::Applied) {
            MarkApplied(snapshot.name, index, textureAddress);
        }
    }
#endif
}

void OnStandaloneTextureAssigned(void* texture) {
#ifndef SILKMODLOADER_ENABLE_RUNTIME_SKIN
    EXL_UNUSED(texture);
    return;
#else
    if (g_ApplyInProgress || texture == nullptr || !EnsureRuntimeState()) {
        return;
    }

    char name[kResourceNameCapacity]{};
    if (!g_TextureReplacer.ReadName(texture, name, sizeof(name))) {
        return;
    }
    ReplacementTexture replacement{};
    const auto resolved = g_Scanner.ResolveStandalone(
        g_SkinRoot, std::string_view(name), &replacement);
    if (resolved.error != SdReadError::None) {
        return;
    }

    const auto textureAddress = reinterpret_cast<std::uintptr_t>(texture);
    if (textureAddress == 0 ||
        WasStandaloneApplied(std::string_view(name), textureAddress)) {
        return;
    }

    unity::TextureMetadata metadata{};
    if (!g_TextureReplacer.ReadMetadata(texture, &metadata)) {
        Logging.Log("[Skin] standalone %s metadata read failed", name);
        return;
    }
    Logging.Log("[Skin] standalone name=%s target=%ux%u format=%d native=%016lx",
                name, metadata.width, metadata.height, metadata.format,
                metadata.nativePointer);

    FileBuffer png{};
    const auto read = g_Scanner.ReadReplacement(replacement, &png);
    if (read.error != SdReadError::None) {
        Logging.Log("[Skin] standalone %s read failed error=%s result=0x%08x",
                    name, SdReadErrorName(read.error), read.result);
        return;
    }

    image::PngInfo pngInfo{};
    if (!image::InspectPng(png.data, png.size, &pngInfo)) {
        Logging.Log("[Skin] standalone %s PNG validation failed", name);
        return;
    }
    if (pngInfo.width != metadata.width || pngInfo.height != metadata.height) {
        Logging.Log("[Skin] standalone %s dimension mismatch png=%ux%u target=%ux%u",
                    name, pngInfo.width, pngInfo.height,
                    metadata.width, metadata.height);
        return;
    }

    g_ApplyInProgress = true;
    const auto operation = g_TextureReplacer.ApplyPng(
        texture, png.data, png.size, replacement.width, replacement.height);
    g_ApplyInProgress = false;
    Logging.Log("[Skin] standalone %s operation=%s", name,
                unity::TextureOperationName(operation));
    if (operation == unity::TextureOperation::Applied) {
        MarkStandaloneApplied(std::string_view(name), textureAddress);
    }
#endif
}

void OnSpriteAssigned(void* sprite) {
#ifndef SILKMODLOADER_ENABLE_RUNTIME_SKIN
    EXL_UNUSED(sprite);
    return;
#else
    if (g_ApplyInProgress || sprite == nullptr || !EnsureRuntimeState()) {
        return;
    }

    void* texture = g_TextureReplacer.GetSpriteTexture(sprite);
    if (texture == nullptr) {
        return;
    }
    char name[kResourceNameCapacity]{};
    if (!g_TextureReplacer.ReadName(texture, name, sizeof(name))) {
        return;
    }
    ReplacementTexture replacement{};
    const auto resolved = g_Scanner.ResolveSpriteTexture(
        g_SkinRoot, std::string_view(name), &replacement);
    if (resolved.error != SdReadError::None) {
        return;
    }

    const auto textureAddress = reinterpret_cast<std::uintptr_t>(texture);
    if (textureAddress == 0 ||
        WasSpriteApplied(std::string_view(name), textureAddress)) {
        return;
    }

    unity::TextureMetadata metadata{};
    if (!g_TextureReplacer.ReadMetadata(texture, &metadata)) {
        Logging.Log("[Skin] sprite-texture %s metadata read failed", name);
        return;
    }
    Logging.Log("[Skin] sprite-texture name=%s target=%ux%u format=%d native=%016lx",
                name, metadata.width, metadata.height, metadata.format,
                metadata.nativePointer);

    FileBuffer png{};
    const auto read = g_Scanner.ReadReplacement(replacement, &png);
    if (read.error != SdReadError::None) {
        Logging.Log("[Skin] sprite-texture %s read failed error=%s result=0x%08x",
                    name, SdReadErrorName(read.error), read.result);
        return;
    }

    image::PngInfo pngInfo{};
    if (!image::InspectPng(png.data, png.size, &pngInfo)) {
        Logging.Log("[Skin] sprite-texture %s PNG validation failed", name);
        return;
    }
    if (pngInfo.width != metadata.width || pngInfo.height != metadata.height) {
        Logging.Log("[Skin] sprite-texture %s dimension mismatch png=%ux%u target=%ux%u",
                    name, pngInfo.width, pngInfo.height,
                    metadata.width, metadata.height);
        return;
    }

    g_ApplyInProgress = true;
    const auto operation = g_TextureReplacer.ApplyPng(
        texture, png.data, png.size, replacement.width, replacement.height);
    g_ApplyInProgress = false;
    Logging.Log("[Skin] sprite-texture %s operation=%s", name,
                unity::TextureOperationName(operation));
    if (operation == unity::TextureOperation::Applied) {
        MarkSpriteApplied(std::string_view(name), textureAddress);
    }
#endif
}

} // namespace silkmodloader::skin
