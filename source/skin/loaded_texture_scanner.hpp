#pragma once

#include "unity/texture_replacer.hpp"
#include <cstdint>

namespace silkmodloader::skin {

/* All entry points run on Unity's main thread. The snapshot is rooted with
 * a normal IL2CPP GCHandle until its final batch has been processed. */
class LoadedTextureScanner final {
public:
    void BindVerifiedMain(std::uintptr_t mainBase);
    void Tick(const unity::TextureReplacer& replacer);

private:
    using Identity = unity::TextureIdentity;
    static constexpr std::size_t kCacheCapacity = 8192;
    /* Two snapshots avoid tombstones/stale entries: keep the previous round
     * while constructing this round, then swap. Linear probing retains
     * colliding objects instead of overwriting each other's cache slot. */
    bool Contains(const Identity* cache, const Identity& identity) const;
    void Remember(const Identity& identity);
    static std::size_t CacheIndex(const Identity& identity);
    using InitializeMetadataFn = void* (*)(std::uintptr_t*, bool);
    using GetTypeFn = void* (*)(std::uintptr_t, const void*);
    using FindAllFn = void* (*)(void*, const void*);
    using NewHandleFn = std::uintptr_t (*)(void*, std::uintptr_t, std::int32_t, const void*);
    using GetTargetFn = void* (*)(std::uintptr_t, const void*);
    using FreeHandleFn = void (*)(std::uintptr_t, const void*);

    bool BeginSnapshot();
    void FinishSnapshot();
    InitializeMetadataFn m_InitializeMetadata{};
    GetTypeFn m_GetType{};
    FindAllFn m_FindAll{};
    NewHandleFn m_NewHandle{};
    GetTargetFn m_GetTarget{};
    FreeHandleFn m_FreeHandle{};
    std::uintptr_t* m_TypeSlot{};
    std::uintptr_t m_Handle{};
    std::uint64_t m_NextScan{};
    std::uint64_t m_TicksPerSecond{};
    std::size_t m_Count{}, m_Index{}, m_New{}, m_Applied{}, m_Retries{};
    unsigned m_Round{};
    unsigned m_PreviousCache{};
    bool m_CacheOverflowReported{};
    Identity m_Seen[2][kCacheCapacity]{};
};

} // namespace silkmodloader::skin
