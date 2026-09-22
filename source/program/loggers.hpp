/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Derived from exlaunch f9f4b0dd07b68f97958cb9c79228bbca22ca80d5.
 * Configure SilkRuntime logging and probe selections.
 * See THIRD_PARTY_NOTICES.md and licenses/source-inventory.json.
 */
#pragma once

#include <lib/log/svc_logger.hpp>
#ifndef SILKMODLOADER_SVC_ONLY_BOOTSTRAP
#include "sd_file_logger.hpp"
#endif

/* Specify logger implementations here. */
#ifdef SILKMODLOADER_SVC_ONLY_BOOTSTRAP
inline exl::log::LoggerMgr<exl::log::SvcLogger> Logging;
#else
inline exl::log::LoggerMgr<
    exl::log::SvcLogger,
    silkmodloader::SdFileLogger
> Logging;
#endif
