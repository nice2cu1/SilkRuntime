#pragma once

#include <cstdint>

namespace silkmodloader::hook {

void BindSpriteRendererSetSpriteOriginal(std::uintptr_t address);
void BindUIImageSetSpriteOriginal(std::uintptr_t address);
std::uintptr_t GetSpriteRendererSetSpriteExternalHookAddress();
std::uintptr_t GetUIImageSetSpriteExternalHookAddress();

extern "C" void SpriteRendererSetSpriteExternalHook(void* renderer,
                                                      void* sprite,
                                                      const void* method);
extern "C" void UIImageSetSpriteExternalHook(void* image, void* sprite,
                                               const void* method);

} // namespace silkmodloader::hook
