#pragma once

#include <cstdint>

namespace silkmodloader::hook {

/*
 * The call-site supplies the tk2d collection in x0 and the IL2CPP
 * MethodInfo pointer in x1.  The original function does not consume x1 in
 * its entry sequence, but retaining the hidden argument keeps this stub
 * ABI-compatible with the generated IL2CPP method signature.
 */
using Tk2dInitFunction = void (*)(void* self, const void* method);

void BindTk2dOriginal(std::uintptr_t address);
std::uintptr_t GetTk2dExternalHookAddress();

extern "C" void Tk2dInitExternalHook(void* self, const void* method);

} // namespace silkmodloader::hook
