#include "sprite_external_hook.hpp"

#include "lib.hpp"
#include "skin/skin_loader.hpp"

namespace silkmodloader::hook {
namespace {

using SetSpriteFunction = void (*)(void*, void*, const void*);

SetSpriteFunction g_SpriteRendererOriginal{};
SetSpriteFunction g_UIImageOriginal{};

} // namespace

void BindSpriteRendererSetSpriteOriginal(std::uintptr_t address) {
    g_SpriteRendererOriginal = reinterpret_cast<SetSpriteFunction>(address);
}

void BindUIImageSetSpriteOriginal(std::uintptr_t address) {
    g_UIImageOriginal = reinterpret_cast<SetSpriteFunction>(address);
}

std::uintptr_t GetSpriteRendererSetSpriteExternalHookAddress() {
    return reinterpret_cast<std::uintptr_t>(
        &SpriteRendererSetSpriteExternalHook);
}

std::uintptr_t GetUIImageSetSpriteExternalHookAddress() {
    return reinterpret_cast<std::uintptr_t>(&UIImageSetSpriteExternalHook);
}

extern "C" void SpriteRendererSetSpriteExternalHook(void* renderer,
                                                      void* sprite,
                                                      const void* method) {
    const auto original = g_SpriteRendererOriginal;
    if (original == nullptr) {
        static constexpr char kUnbound[] =
            "[ExternalHook][ERROR] SpriteRenderer.set_sprite original is not "
            "bound; call skipped.\n";
        svcOutputDebugString(kUnbound, sizeof(kUnbound) - 1);
        return;
    }
    original(renderer, sprite, method);
    silkmodloader::skin::OnSpriteAssigned(sprite);
}

extern "C" void UIImageSetSpriteExternalHook(void* image, void* sprite,
                                               const void* method) {
    const auto original = g_UIImageOriginal;
    if (original == nullptr) {
        static constexpr char kUnbound[] =
            "[ExternalHook][ERROR] UI.Image.set_sprite original is not bound; "
            "call skipped.\n";
        svcOutputDebugString(kUnbound, sizeof(kUnbound) - 1);
        return;
    }
    original(image, sprite, method);
    silkmodloader::skin::OnSpriteAssigned(sprite);
}

} // namespace silkmodloader::hook
