#pragma once

#include <cstddef>
#include <cstdint>

#include <nn/fs.hpp>

namespace silkmodloader::skin {

enum class SdReadError : std::uint8_t {
    None,
    NotInitialized,
    MissingSymbol,
    OpenFailed,
    SizeFailed,
    FileTooLarge,
    AllocationFailed,
    ReadFailed,
    NoDirectory,
    MultipleDirectories,
};

struct FileBuffer {
    FileBuffer() = default;
    ~FileBuffer();

    FileBuffer(const FileBuffer&) = delete;
    FileBuffer& operator=(const FileBuffer&) = delete;

    void Reset();

    std::uint8_t* data{};
    std::size_t size{};
};

struct SdReaderStatus {
    SdReadError error{SdReadError::None};
    Result result{};
};

class SdFileReader final {
public:
    static constexpr std::size_t kMaxFileSize = 32 * 1024 * 1024;

    /* The caller must verify the exact game Build ID before binding this. */
    void BindVerifiedMain(std::uintptr_t mainBase);
    SdReaderStatus Initialize();
    SdReaderStatus ReadAll(const char* path, FileBuffer* output,
                           std::size_t maximumSize = kMaxFileSize);

    /* Read a small prefix without allocating the whole file.  The runtime
     * manifest scan uses this for PNG headers so a large set of replacement
     * images cannot exhaust or fragment the game heap before gameplay starts. */
    SdReaderStatus ReadPrefix(const char* path, void* output,
                              std::size_t byteCount,
                              std::size_t maximumSize = kMaxFileSize,
                              std::size_t* totalSize = nullptr);

    /* Resolve the single skin directory below a fixed Mods/Skin root. */
    SdReaderStatus FindSingleDirectory(const char* parentPath,
                                        char* outputPath,
                                        std::size_t outputCapacity);

private:
    using LookupSymbolFn = Result (*)(std::uint64_t*, const char*);
    using OpenFileFn = Result (*)(nn::fs::FileHandle*, const char*, int);
    using CloseFileFn = void (*)(nn::fs::FileHandle);
    using GetFileSizeFn = Result (*)(long*, nn::fs::FileHandle);
    using ReadFileFn = Result (*)(nn::fs::FileHandle, long, void*, ulong);
    using OpenDirectoryFn = Result (*)(nn::fs::DirectoryHandle*, const char*, int);
    using CloseDirectoryFn = void (*)(nn::fs::DirectoryHandle);
    using ReadDirectoryFn = Result (*)(long*, nn::fs::DirectoryEntry*,
                                       nn::fs::DirectoryHandle, long);
    using GetDirectoryEntryCountFn = Result (*)(long*, nn::fs::DirectoryHandle);

    LookupSymbolFn m_LookupSymbol{};
    OpenFileFn m_Open{};
    CloseFileFn m_Close{};
    GetFileSizeFn m_GetFileSize{};
    ReadFileFn m_Read{};
    OpenDirectoryFn m_OpenDirectory{};
    CloseDirectoryFn m_CloseDirectory{};
    ReadDirectoryFn m_ReadDirectory{};
    GetDirectoryEntryCountFn m_GetDirectoryEntryCount{};
    bool m_Initialized{};
};

const char* SdReadErrorName(SdReadError error);

} // namespace silkmodloader::skin
