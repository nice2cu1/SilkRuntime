#pragma once

#include <lib/log/ilogger.hpp>
#include <nn/fs.hpp>

namespace silkmodloader {

struct SdFileLoggerInitResult {
    Result mountResult{0xffffffff};
    Result openResult{0xffffffff};
    const char* missingSymbol{};
    bool enabled{};
};

class SdFileLogger final : public exl::log::ILogger {
public:
    static constexpr const char* LogPath = "sd:/SilkModLoader/SilkModLoader.log";

    SdFileLoggerInitResult Initialize();
    void LogRaw(std::string_view data) final;

private:
    using WriteFileFn = Result (*)(
        nn::fs::FileHandle,
        s64,
        const void*,
        u64,
        const nn::fs::WriteOption&
    );

    nn::fs::FileHandle m_Handle{};
    WriteFileFn m_WriteFile{};
    s64 m_Position{};
    bool m_Enabled{};
};

} // namespace silkmodloader
