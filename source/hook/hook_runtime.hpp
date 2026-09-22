#pragma once

#include "hook_target.hpp"

namespace silkmodloader::hook {

/*
 * Read-only first phase. This computes and logs the patch plan, accepts either
 * the original call-site instruction or a validated prelaunch exefs patch, and
 * never writes a game instruction or initializes exlaunch's patcher.
 */
void EmitReadOnlyHookPlan(const char* buildId, const HookTarget& target);

void EmitReadOnlyMaterialHookPlan(const char* buildId,
                                  const HookTarget& target);

void EmitReadOnlySpriteRendererHookPlan(const char* buildId,
                                        const HookTarget& target);

void EmitReadOnlyUIImageHookPlan(const char* buildId,
                                 const HookTarget& target);

} // namespace silkmodloader::hook
