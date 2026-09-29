#include "skin_loader.hpp"

#include "image/png_loader.hpp"
#include "loaded_texture_scanner.hpp"
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
LoadedTextureScanner g_LoadedTextures;
bool g_DiscoveryInProgress{};
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

struct AppliedNamedRecord {
    char name[kResourceNameCapacity]{};
    std::uintptr_t native{};
    std::int32_t instanceId{};
};

AppliedNamedRecord g_AppliedNamed[512]{};
std::size_t g_NextAppliedNamed{};

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
    g_LoadedTextures.BindVerifiedMain(mainBase);

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

bool WasNamedApplied(std::string_view name,
                     const unity::TextureIdentity& identity) {
    for (const auto& record : g_AppliedNamed) {
        if (record.native == identity.nativePointer &&
            record.instanceId == identity.instanceId &&
            std::string_view(record.name) == name) return true;
    }
    return false;
}

void MarkNamedApplied(std::string_view name,
                      const unity::TextureMetadata& metadata) {
    auto& record = g_AppliedNamed[g_NextAppliedNamed++ % 512];
    std::memcpy(record.name, name.data(), name.size());
    record.name[name.size()] = '\0';
    record.native = metadata.nativePointer;
    record.instanceId = metadata.instanceId;
}

TextureVisitResult ApplyNamedTexture(void* texture, const char* observer,
                                    const char* spriteName = nullptr) {
    unity::TextureIdentity identity{};
    if (!g_TextureReplacer.ReadIdentity(texture, &identity)) return TextureVisitResult::Unchanged;
    char name[kResourceNameCapacity]{};
    if (!g_TextureReplacer.ReadName(texture, name, sizeof(name))) {
        return TextureVisitResult::Unchanged;
    }

    if (WasNamedApplied(name, identity)) return TextureVisitResult::Unchanged;
    ReplacementTexture replacement{};
    auto resolved = g_Scanner.ResolveStandalone(g_SkinRoot, name, &replacement);
    const char* source = "standalone";
    if (resolved.error != SdReadError::None) {
        resolved = g_Scanner.ResolveSpriteTexture(g_SkinRoot, name, &replacement);
        source = "sprite";
    }
    // A whole atlas cannot be safely applied to a differently named texture
    // using only a sprite name and matching dimensions: the UV layout may differ.
    if (resolved.error != SdReadError::None) return TextureVisitResult::Unchanged;
    unity::TextureMetadata metadata{};
    if (!g_TextureReplacer.ReadMetadata(texture, &metadata)) {
        Logging.Log("[Skin] texture=%s metadata unavailable; retry later", name);
        return TextureVisitResult::Retry;
    }
    FileBuffer png{};
    const auto read = g_Scanner.ReadReplacement(replacement, &png);
    if (read.error != SdReadError::None) {
        Logging.Log("[Skin] texture=%s read failed error=%s result=0x%08x",
                    name, SdReadErrorName(read.error), read.result);
        return TextureVisitResult::Retry;
    }
    image::PngInfo pngInfo{};
    if (!image::InspectPng(png.data, png.size, &pngInfo) ||
        pngInfo.width != metadata.width || pngInfo.height != metadata.height) {
        Logging.Log("[Skin] texture=%s PNG rejected png=%ux%u target=%ux%u",
                    name, pngInfo.width, pngInfo.height, metadata.width, metadata.height);
        return TextureVisitResult::Unchanged;
    }
    g_ApplyInProgress = true;
    const auto operation = g_TextureReplacer.ApplyPng(
        texture, png.data, png.size, replacement.width, replacement.height);
    g_ApplyInProgress = false;
    Logging.Log("[Skin] texture=%s observer=%s source=%s operation=%s sprite=%s target=%ux%u format=%d instance=%d",
                name, observer, source, unity::TextureOperationName(operation),
                spriteName ? spriteName : "<none>", metadata.width, metadata.height,
                metadata.format, metadata.instanceId);
    if (operation == unity::TextureOperation::Applied) {
        g_TextureReplacer.ReadMetadata(texture, &metadata);
        MarkNamedApplied(name, metadata);
        return TextureVisitResult::Applied;
    }
    return operation == unity::TextureOperation::LoadFailed
        ? TextureVisitResult::Retry : TextureVisitResult::Unchanged;
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
        Logging.Log("[Skin] %s atlas%u operation=%s target=%ux%u format=%d instance=%d",
                    snapshot.name, index, unity::TextureOperationName(operation),
                    metadata.width, metadata.height, metadata.format, metadata.instanceId);
        if (operation == unity::TextureOperation::Applied) {
            MarkApplied(snapshot.name, index, textureAddress);
        }
    }
#endif
}

void OnStandaloneTextureAssigned(void* texture) {
#ifndef SILKMODLOADER_ENABLE_RUNTIME_SKIN
    EXL_UNUSED(texture);
#else
    if (!g_ApplyInProgress && texture != nullptr && EnsureRuntimeState()) {
        ApplyNamedTexture(texture, "material");
    }
#endif
}

void OnSpriteAssigned(void* sprite) {
#ifndef SILKMODLOADER_ENABLE_RUNTIME_SKIN
    EXL_UNUSED(sprite);
#else
    if (g_ApplyInProgress || sprite == nullptr || !EnsureRuntimeState()) return;
    void* texture = g_TextureReplacer.GetSpriteTexture(sprite);
    if (texture == nullptr) return;
    char spriteName[kResourceNameCapacity]{};
    g_TextureReplacer.ReadName(sprite, spriteName, sizeof(spriteName));
    ApplyNamedTexture(texture, "sprite", spriteName);
#endif
}

TextureVisitResult OnLoadedTextureDiscovered(void* texture) {
#ifndef SILKMODLOADER_ENABLE_RUNTIME_SKIN
    EXL_UNUSED(texture);
    return TextureVisitResult::Unchanged;
#else
    if (g_ApplyInProgress || !EnsureRuntimeState()) return TextureVisitResult::Retry;
    return ApplyNamedTexture(texture, "discovery");
#endif
}

void OnMainThreadFrame() {
#ifdef SILKMODLOADER_ENABLE_RUNTIME_SKIN
    if (g_ApplyInProgress || g_DiscoveryInProgress || !EnsureRuntimeState()) return;
    g_DiscoveryInProgress = true;
    g_LoadedTextures.Tick(g_TextureReplacer);
    g_DiscoveryInProgress = false;
#endif
}

} // namespace silkmodloader::skin
