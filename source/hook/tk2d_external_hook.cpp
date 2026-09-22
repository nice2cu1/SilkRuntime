#include "tk2d_external_hook.hpp"

#include "lib.hpp"
#include "skin/skin_loader.hpp"

namespace silkmodloader::hook {
namespace {

Tk2dInitFunction g_Original{};
bool g_HitLogged{};

} // namespace

void BindTk2dOriginal(std::uintptr_t address) {
    g_Original = reinterpret_cast<Tk2dInitFunction>(address);
}

std::uintptr_t GetTk2dExternalHookAddress() {
    return reinterpret_cast<std::uintptr_t>(&Tk2dInitExternalHook);
}

extern "C" void Tk2dInitExternalHook(void* self, const void* method) {
    const auto original = g_Original;
    if (original == nullptr) {
        static constexpr char kUnbound[] =
            "[ExternalHook][ERROR] tk2d Init original is not bound; "
            "call skipped.\n";
        svcOutputDebugString(kUnbound, sizeof(kUnbound) - 1);
        return;
    }

    /* Preserve the original game behavior before the diagnostic observer. */
    original(self, method);

    silkmodloader::skin::OnCollectionInitialized(self);

    if (!g_HitLogged) {
        g_HitLogged = true;
        static constexpr char kHit[] =
            "[ExternalHook] tk2dSpriteCollectionData::Init HOOK HIT; "
            "original preserved.\n";
        svcOutputDebugString(kHit, sizeof(kHit) - 1);
    }
}

} // namespace silkmodloader::hook
