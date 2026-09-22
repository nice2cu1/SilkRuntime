#pragma once

#include <cstdint>

namespace silkmodloader::hook {

void BindMaterialSetMainTextureOriginal(std::uintptr_t address);
std::uintptr_t GetMaterialSetMainTextureExternalHookAddress();

/* Matches UnityEngine.Material.set_mainTexture on the verified IL2CPP build. */
extern "C" void MaterialSetMainTextureExternalHook(void* material,
                                                     void* texture,
                                                     const void* method);

} // namespace silkmodloader::hook
