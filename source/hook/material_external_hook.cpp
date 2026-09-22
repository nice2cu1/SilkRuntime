#include "material_external_hook.hpp"

#include "lib.hpp"
#include "skin/skin_loader.hpp"

namespace silkmodloader::hook {
namespace {

using MaterialSetMainTextureFunction = void (*)(void*, void*, const void*);

MaterialSetMainTextureFunction g_Original{};

} // namespace

void BindMaterialSetMainTextureOriginal(std::uintptr_t address) {
    g_Original = reinterpret_cast<MaterialSetMainTextureFunction>(address);
}

std::uintptr_t GetMaterialSetMainTextureExternalHookAddress() {
    return reinterpret_cast<std::uintptr_t>(&MaterialSetMainTextureExternalHook);
}

extern "C" void MaterialSetMainTextureExternalHook(void* material,
                                                     void* texture,
                                                     const void* method) {
    const auto original = g_Original;
    if (original == nullptr) {
        static constexpr char kUnbound[] =
            "[ExternalHook][ERROR] Material.set_mainTexture original is not "
            "bound; call skipped.\n";
        svcOutputDebugString(kUnbound, sizeof(kUnbound) - 1);
        return;
    }

    original(material, texture, method);
    silkmodloader::skin::OnStandaloneTextureAssigned(texture);
}

} // namespace silkmodloader::hook
