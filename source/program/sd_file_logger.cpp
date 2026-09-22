#include "sd_file_logger.hpp"

#include <rtld.hpp>

namespace silkmodloader {

namespace {

template <typename Function>
Function Resolve(const char* name) {
    return reinterpret_cast<Function>(rtld::lookup_global_auto(name));
}

using MountSdCardForDebugFn = Result (*)(const char*);
using CreateDirectoryFn = Result (*)(const char*);
using CreateFileFn = Result (*)(const char*, s64);
using OpenFileFn = Result (*)(nn::fs::FileHandle*, const char*, int);
using CloseFileFn = void (*)(nn::fs::FileHandle);
using GetFileSizeFn = Result (*)(long*, nn::fs::FileHandle);
using WriteFileFn = Result (*)(
    nn::fs::FileHandle,
    s64,
    const void*,
    u64,
    const nn::fs::WriteOption&
);

} // namespace

SdFileLoggerInitResult SdFileLogger::Initialize() {
    SdFileLoggerInitResult result{};

    const auto resolveRequired = [&result]<typename Function>(
        Function& output, const char* symbol
    ) {
        output = Resolve<Function>(symbol);
        if (output == nullptr && result.missingSymbol == nullptr) {
            result.missingSymbol = symbol;
        }
    };

    MountSdCardForDebugFn mountSdCardForDebug{};
    CreateDirectoryFn createDirectory{};
    CreateFileFn createFile{};
    OpenFileFn openFile{};
    CloseFileFn closeFile{};
    GetFileSizeFn getFileSize{};
    WriteFileFn writeFile{};

    resolveRequired(mountSdCardForDebug, "_ZN2nn2fs19MountSdCardForDebugEPKc");
    resolveRequired(createDirectory, "_ZN2nn2fs15CreateDirectoryEPKc");
    resolveRequired(createFile, "_ZN2nn2fs10CreateFileEPKcl");
    resolveRequired(openFile, "_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci");
    resolveRequired(closeFile, "_ZN2nn2fs9CloseFileENS0_10FileHandleE");
    resolveRequired(getFileSize, "_ZN2nn2fs11GetFileSizeEPlNS0_10FileHandleE");
    resolveRequired(
        writeFile,
        "_ZN2nn2fs9WriteFileENS0_10FileHandleElPKvmRKNS0_11WriteOptionE"
    );
    if (result.missingSymbol != nullptr) {
        return result;
    }

    result.mountResult = mountSdCardForDebug("sd");
    if (result.mountResult != 0) {
        return result;
    }

    // Both operations can legitimately fail when the directory/file already
    // exists, so OpenFile is the authoritative readiness check.
    createDirectory("sd:/SilkModLoader");
    createFile(LogPath, 0);

    result.openResult = openFile(
        &m_Handle,
        LogPath,
        nn::fs::OpenMode_Write | nn::fs::OpenMode_Append
    );
    if (result.openResult != 0) {
        return result;
    }

    long size = 0;
    const Result sizeResult = getFileSize(&size, m_Handle);
    if (sizeResult != 0 || size < 0) {
        closeFile(m_Handle);
        result.openResult = sizeResult;
        return result;
    }

    m_WriteFile = writeFile;
    m_Position = size;
    m_Enabled = true;
    result.enabled = true;
    return result;
}

void SdFileLogger::LogRaw(std::string_view data) {
    if (!m_Enabled) {
        return;
    }

    const auto noFlush = nn::fs::WriteOption::CreateOption(0);
    const auto flush = nn::fs::WriteOption::CreateOption(
        nn::fs::WriteOptionFlag_Flush
    );

    Result writeResult = m_WriteFile(
        m_Handle, m_Position, data.data(), data.size(), noFlush
    );
    if (writeResult != 0) {
        m_Enabled = false;
        return;
    }
    m_Position += static_cast<s64>(data.size());

    constexpr char newline = '\n';
    writeResult = m_WriteFile(
        m_Handle, m_Position, &newline, sizeof(newline), flush
    );
    if (writeResult != 0) {
        m_Enabled = false;
        return;
    }
    m_Position += sizeof(newline);
}

} // namespace silkmodloader
