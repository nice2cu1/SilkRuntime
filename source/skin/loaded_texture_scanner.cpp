#include "loaded_texture_scanner.hpp"
#include "skin_loader.hpp"
#include "program/offsets.hpp"
#include "lib.hpp"

namespace silkmodloader::skin {

std::size_t LoadedTextureScanner::CacheIndex(const Identity& identity) {
    // Mix all pointer/ID bits; Unity instance IDs can have patterned low bits.
    std::uint64_t value = identity.nativePointer ^
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(identity.instanceId)) << 32);
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31;
    return value & (kCacheCapacity - 1);
}

bool LoadedTextureScanner::Contains(const Identity* cache, const Identity& identity) const {
    const auto start = CacheIndex(identity);
    for (std::size_t probe = 0; probe < kCacheCapacity; ++probe) {
        const auto& entry = cache[(start + probe) & (kCacheCapacity - 1)];
        if (entry.nativePointer == 0) return false;
        if (entry.nativePointer == identity.nativePointer &&
            entry.instanceId == identity.instanceId) return true;
    }
    return false;
}

void LoadedTextureScanner::Remember(const Identity& identity) {
    auto* cache = m_Seen[m_PreviousCache ^ 1];
    const auto start = CacheIndex(identity);
    for (std::size_t probe = 0; probe < kCacheCapacity; ++probe) {
        auto& entry = cache[(start + probe) & (kCacheCapacity - 1)];
        if (entry.nativePointer == 0 ||
            (entry.nativePointer == identity.nativePointer && entry.instanceId == identity.instanceId)) {
            entry = identity;
            return;
        }
    }
    // Capacity exhaustion may cause extra lookups, never skip a replacement.
    if (!m_CacheOverflowReported) {
        Logging.Log("[SkinScan] identity cache full capacity=%lu; uncached objects remain eligible",
                    kCacheCapacity);
        m_CacheOverflowReported = true;
    }
}

void LoadedTextureScanner::BindVerifiedMain(std::uintptr_t mainBase) {
    const auto& offsets = game::kLoadedTextureOffsets;
    m_InitializeMetadata = reinterpret_cast<InitializeMetadataFn>(mainBase + offsets.initialize_runtime_metadata);
    m_GetType = reinterpret_cast<GetTypeFn>(mainBase + offsets.type_get_type_from_handle);
    m_FindAll = reinterpret_cast<FindAllFn>(mainBase + offsets.resources_find_objects_of_type_all);
    m_NewHandle = reinterpret_cast<NewHandleFn>(mainBase + offsets.gc_handle_get_target_handle);
    m_GetTarget = reinterpret_cast<GetTargetFn>(mainBase + offsets.gc_handle_get_target);
    m_FreeHandle = reinterpret_cast<FreeHandleFn>(mainBase + offsets.gc_handle_free_handle);
    m_TypeSlot = reinterpret_cast<std::uintptr_t*>(mainBase + offsets.texture2d_type_slot);
    asm volatile("mrs %0, cntfrq_el0" : "=r"(m_TicksPerSecond));
}

bool LoadedTextureScanner::BeginSnapshot() {
    m_NextScan = svcGetSystemTick() + m_TicksPerSecond;
    m_InitializeMetadata(m_TypeSlot, true);
    const auto typeHandle = *m_TypeSlot;
    if (typeHandle == 0 || (typeHandle & 1) != 0) {
        Logging.Log("[SkinScan] Texture2D type metadata unresolved");
        return false;
    }
    void* type = m_GetType(typeHandle, nullptr);
    if (type == nullptr) {
        Logging.Log("[SkinScan] Texture2D System.Type lookup failed");
        return false;
    }
    void* snapshot = m_FindAll(type, nullptr);
    if (snapshot == nullptr) {
        Logging.Log("[SkinScan] Texture2D enumeration returned null");
        return false;
    }
    // GCHandleType.Normal = 2 in this executable. Never leave an unrooted
    // managed snapshot in module globals while returning to the game.
    m_Handle = m_NewHandle(snapshot, 0, 2, nullptr);
    if (m_Handle == 0) {
        Logging.Log("[SkinScan] snapshot GCHandle allocation failed");
        return false;
    }
    m_Count = *reinterpret_cast<const std::uintptr_t*>(
        reinterpret_cast<std::uintptr_t>(snapshot) + 0x18);
    m_Index = m_New = m_Applied = m_Retries = 0;
    for (auto& entry : m_Seen[m_PreviousCache ^ 1]) entry = {};
    ++m_Round;
    if (m_Count > 1000000) {
        Logging.Log("[SkinScan] implausible array length=%lu; snapshot refused", m_Count);
        FinishSnapshot();
        return false;
    }
    return true;
}

void LoadedTextureScanner::FinishSnapshot() {
    m_FreeHandle(m_Handle, nullptr);
    m_Handle = 0;
    m_PreviousCache ^= 1;
    m_NextScan = svcGetSystemTick() + m_TicksPerSecond * (m_Retries ? 5 : 1);
    if (m_Round == 1 || m_Applied != 0 || m_Retries != 0) {
        Logging.Log("[SkinScan] round=%u textures=%lu cacheMisses=%lu applied=%lu retry=%lu",
                    m_Round, m_Count, m_New, m_Applied, m_Retries);
    }
}

void LoadedTextureScanner::Tick(const unity::TextureReplacer& replacer) {
    if (m_TypeSlot == nullptr || m_TicksPerSecond == 0) return;
    const auto started = svcGetSystemTick();
    if (m_Handle == 0) {
        if (started < m_NextScan || !BeginSnapshot()) return;
        // Enumeration is indivisible; do not also load a PNG this frame.
        return;
    }
    void* snapshot = m_GetTarget(m_Handle, nullptr);
    if (snapshot == nullptr) {
        Logging.Log("[SkinScan] rooted snapshot lost");
        FinishSnapshot();
        return;
    }
    auto* entries = reinterpret_cast<void**>(
        reinterpret_cast<std::uintptr_t>(snapshot) + 0x20);
    for (unsigned batch = 0; m_Index < m_Count && batch < 64; ++batch) {
        if (batch != 0 && svcGetSystemTick() - started >= m_TicksPerSecond / 500) break;
        void* texture = entries[m_Index++];
        Identity identity{};
        // Cached objects need only identity, not width/height/format/name or I/O.
        // A managed wrapper change alone does not create a new native texture.
        if (!replacer.ReadIdentity(texture, &identity)) continue;
        if (Contains(m_Seen[m_PreviousCache ^ 1], identity)) continue;
        if (Contains(m_Seen[m_PreviousCache], identity)) {
            Remember(identity);
            continue;
        }
        ++m_New;
        const auto result = OnLoadedTextureDiscovered(texture);
        if (result == TextureVisitResult::Retry) {
            ++m_Retries;
        } else {
            if (replacer.ReadIdentity(texture, &identity)) Remember(identity);
        }
        if (result == TextureVisitResult::Applied) {
            ++m_Applied;
            break; // At most one new PNG per frame.
        }
    }
    if (m_Index == m_Count) FinishSnapshot();
}

} // namespace silkmodloader::skin
