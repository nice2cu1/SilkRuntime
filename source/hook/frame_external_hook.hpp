#pragma once

#include <cstdint>

namespace silkmodloader::hook {
void BindGameUpdateOriginal(std::uintptr_t address);
std::uintptr_t GetGameUpdateExternalHookAddress();
extern "C" void GameUpdateExternalHook(void* manager, const void* method);
}
