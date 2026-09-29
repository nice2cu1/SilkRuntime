#include "frame_external_hook.hpp"
#include "skin/skin_loader.hpp"
#include "lib.hpp"

namespace silkmodloader::hook {
namespace {
using UpdateFunction = void (*)(void*, const void*);
UpdateFunction g_Original{};
}

void BindGameUpdateOriginal(std::uintptr_t address) {
    g_Original = reinterpret_cast<UpdateFunction>(address);
}

std::uintptr_t GetGameUpdateExternalHookAddress() {
    return reinterpret_cast<std::uintptr_t>(&GameUpdateExternalHook);
}

extern "C" void GameUpdateExternalHook(void* manager, const void* method) {
    if (g_Original == nullptr) {
        static bool reported{};
        if (!reported) {
            Logging.Log("[ExternalHook][ERROR] GameManager frame original is unbound");
            reported = true;
        }
        return;
    }
    g_Original(manager, method);
    skin::OnMainThreadFrame();
}
}
