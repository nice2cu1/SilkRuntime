#pragma once

#include <cstdint>

namespace silkmodloader::hook {

void BindSpriteRendererSetSpriteOriginal(std::uintptr_t address);
void BindUIImageSetSpriteOriginal(std::uintptr_t address);
void BindSpriteAtlasGetSpriteOriginal(std::uintptr_t address);
std::uintptr_t GetSpriteRendererSetSpriteExternalHookAddress();
std::uintptr_t GetUIImageSetSpriteExternalHookAddress();
std::uintptr_t GetSpriteAtlasGetSpriteExternalHookAddress();

extern "C" void SpriteRendererSetSpriteExternalHook(void* renderer,
                                                      void* sprite,
                                                      const void* method);
extern "C" void UIImageSetSpriteExternalHook(void* image, void* sprite,
                                               const void* method);
extern "C" void* SpriteAtlasGetSpriteExternalHook(void* atlas, void* name,
                                                    const void* method);

} // namespace silkmodloader::hook
