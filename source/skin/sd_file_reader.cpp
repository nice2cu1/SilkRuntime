#include "sd_file_reader.hpp"

#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace silkmodloader::skin {
namespace {

/* Proven in the H1k native nn::socket probe for this exact Silksong main. */
constexpr std::uintptr_t kMainLookupSymbolRva = 0x213e4e0;
constexpr const char* kOpenSymbol = "_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci";
constexpr const char* kCloseSymbol = "_ZN2nn2fs9CloseFileENS0_10FileHandleE";
constexpr const char* kSizeSymbol = "_ZN2nn2fs11GetFileSizeEPlNS0_10FileHandleE";
constexpr const char* kReadSymbol = "_ZN2nn2fs8ReadFileENS0_10FileHandleElPvm";
constexpr const char* kOpenDirectorySymbol =
    "_ZN2nn2fs13OpenDirectoryEPNS0_15DirectoryHandleEPKci";
constexpr const char* kCloseDirectorySymbol =
    "_ZN2nn2fs14CloseDirectoryENS0_15DirectoryHandleE";
constexpr const char* kReadDirectorySymbol =
    "_ZN2nn2fs13ReadDirectoryEPlPNS0_14DirectoryEntryENS0_15DirectoryHandleEl";
constexpr const char* kGetDirectoryEntryCountSymbol =
    "_ZN2nn2fs22GetDirectoryEntryCountEPlNS0_15DirectoryHandleE";

constexpr std::size_t kMaxDirectoryEntries = 256;

SdReaderStatus MakeStatus(SdReadError error, Result result = 0) {
    return {.error = error, .result = result};
}

} // namespace

void SdFileReader::BindVerifiedMain(std::uintptr_t mainBase) {
    m_LookupSymbol = mainBase == 0
        ? nullptr
        : reinterpret_cast<LookupSymbolFn>(mainBase + kMainLookupSymbolRva);
    m_Open = nullptr;
    m_Close = nullptr;
    m_GetFileSize = nullptr;
    m_Read = nullptr;
    m_OpenDirectory = nullptr;
    m_CloseDirectory = nullptr;
    m_ReadDirectory = nullptr;
    m_GetDirectoryEntryCount = nullptr;
    m_Initialized = false;
}

FileBuffer::~FileBuffer() {
    Reset();
}

void FileBuffer::Reset() {
    if (data != nullptr) {
        std::free(data);
    }
    data = nullptr;
    size = 0;
}

SdReaderStatus SdFileReader::Initialize() {
    if (m_Initialized) {
        return MakeStatus(SdReadError::None);
    }

    if (m_LookupSymbol == nullptr) {
        return MakeStatus(SdReadError::MissingSymbol);
    }

    const auto resolve = [this](auto& output, const char* symbol) {
        std::uint64_t address = 0;
        const Result result = m_LookupSymbol(&address, symbol);
        if (result != 0 || address == 0) {
            output = nullptr;
            return false;
        }
        output = reinterpret_cast<std::remove_reference_t<decltype(output)>>(address);
        return true;
    };

    const bool symbolsResolved =
        resolve(m_Open, kOpenSymbol) &&
        resolve(m_Close, kCloseSymbol) &&
        resolve(m_GetFileSize, kSizeSymbol) &&
        resolve(m_Read, kReadSymbol) &&
        resolve(m_OpenDirectory, kOpenDirectorySymbol) &&
        resolve(m_CloseDirectory, kCloseDirectorySymbol) &&
        resolve(m_ReadDirectory, kReadDirectorySymbol) &&
        resolve(m_GetDirectoryEntryCount, kGetDirectoryEntryCountSymbol);
    if (!symbolsResolved) {
        return MakeStatus(SdReadError::MissingSymbol);
    }
    if (m_Open == nullptr || m_Close == nullptr || m_GetFileSize == nullptr ||
        m_Read == nullptr || m_OpenDirectory == nullptr ||
        m_CloseDirectory == nullptr || m_ReadDirectory == nullptr ||
        m_GetDirectoryEntryCount == nullptr) {
        return MakeStatus(SdReadError::MissingSymbol);
    }

    m_Initialized = true;
    return MakeStatus(SdReadError::None);
}

SdReaderStatus SdFileReader::FindSingleDirectory(const char* parentPath,
                                                  char* outputPath,
                                                  std::size_t outputCapacity) {
    if (parentPath == nullptr || outputPath == nullptr || outputCapacity == 0) {
        return MakeStatus(SdReadError::NotInitialized);
    }
    outputPath[0] = '\0';

    const auto init = Initialize();
    if (init.error != SdReadError::None) {
        return init;
    }

    nn::fs::DirectoryHandle handle{};
    const Result openResult = m_OpenDirectory(
        &handle, parentPath, nn::fs::OpenDirectoryMode_Directory);
    if (openResult != 0) {
        return MakeStatus(SdReadError::OpenFailed, openResult);
    }

    long entryCount = 0;
    const Result countResult = m_GetDirectoryEntryCount(&entryCount, handle);
    if (countResult != 0) {
        m_CloseDirectory(handle);
        return MakeStatus(SdReadError::ReadFailed, countResult);
    }
    if (entryCount <= 0) {
        m_CloseDirectory(handle);
        return MakeStatus(SdReadError::NoDirectory);
    }
    if (static_cast<unsigned long>(entryCount) > kMaxDirectoryEntries) {
        m_CloseDirectory(handle);
        return MakeStatus(SdReadError::ReadFailed);
    }

    const auto entryBytes = static_cast<std::size_t>(entryCount) *
                             sizeof(nn::fs::DirectoryEntry);
    auto* entries = static_cast<nn::fs::DirectoryEntry*>(
        std::malloc(entryBytes));
    if (entries == nullptr) {
        m_CloseDirectory(handle);
        return MakeStatus(SdReadError::AllocationFailed);
    }

    long readCount = 0;
    const Result readResult = m_ReadDirectory(
        &readCount, entries, handle, entryCount);
    m_CloseDirectory(handle);
    if (readResult != 0 || readCount < 0 || readCount > entryCount) {
        std::free(entries);
        return MakeStatus(SdReadError::ReadFailed, readResult);
    }

    const nn::fs::DirectoryEntry* selected = nullptr;
    std::size_t directoryCount = 0;
    for (long index = 0; index < readCount; ++index) {
        const auto& entry = entries[index];
        if (entry.m_Type != nn::fs::DirectoryEntryType_Directory) {
            continue;
        }
        selected = &entry;
        ++directoryCount;
    }

    if (directoryCount == 0) {
        std::free(entries);
        return MakeStatus(SdReadError::NoDirectory);
    }
    if (directoryCount != 1 || selected == nullptr) {
        std::free(entries);
        return MakeStatus(SdReadError::MultipleDirectories);
    }

    const auto nameEnd = static_cast<const char*>(std::memchr(
        selected->m_Name, '\0', sizeof(selected->m_Name)));
    if (nameEnd == nullptr) {
        std::free(entries);
        return MakeStatus(SdReadError::ReadFailed);
    }
    const std::size_t parentLength = std::strlen(parentPath);
    const bool hasSeparator = parentLength != 0 &&
                              (parentPath[parentLength - 1] == '/' ||
                               parentPath[parentLength - 1] == '\\');
    const std::size_t nameLength = static_cast<std::size_t>(nameEnd -
                                                              selected->m_Name);
    const std::size_t totalLength = parentLength +
        (hasSeparator ? 0 : 1) + nameLength;
    if (totalLength + 1 > outputCapacity) {
        std::free(entries);
        return MakeStatus(SdReadError::FileTooLarge);
    }

    std::memcpy(outputPath, parentPath, parentLength);
    std::size_t cursor = parentLength;
    if (!hasSeparator) {
        outputPath[cursor++] = '/';
    }
    std::memcpy(outputPath + cursor, selected->m_Name, nameLength);
    outputPath[totalLength] = '\0';
    std::free(entries);
    return MakeStatus(SdReadError::None);
}

SdReaderStatus SdFileReader::ReadAll(const char* path, FileBuffer* output,
                                      std::size_t maximumSize) {
    if (output == nullptr || path == nullptr) {
        return MakeStatus(SdReadError::NotInitialized);
    }
    output->Reset();

    const auto init = Initialize();
    if (init.error != SdReadError::None) {
        return init;
    }

    nn::fs::FileHandle handle{};
    const Result openResult = m_Open(&handle, path, nn::fs::OpenMode_Read);
    if (openResult != 0) {
        return MakeStatus(SdReadError::OpenFailed, openResult);
    }

    long fileSize = 0;
    const Result sizeResult = m_GetFileSize(&fileSize, handle);
    if (sizeResult != 0) {
        m_Close(handle);
        return MakeStatus(SdReadError::SizeFailed, sizeResult);
    }
    if (fileSize < 0 || static_cast<std::uint64_t>(fileSize) > maximumSize) {
        m_Close(handle);
        return MakeStatus(SdReadError::FileTooLarge);
    }

    const auto byteCount = static_cast<std::size_t>(fileSize);
    if (byteCount != 0) {
        output->data = static_cast<std::uint8_t*>(std::malloc(byteCount));
        if (output->data == nullptr) {
            m_Close(handle);
            return MakeStatus(SdReadError::AllocationFailed);
        }

        const Result readResult = m_Read(
            handle, 0, output->data, static_cast<ulong>(byteCount));
        if (readResult != 0) {
            m_Close(handle);
            output->Reset();
            return MakeStatus(SdReadError::ReadFailed, readResult);
        }
    }

    m_Close(handle);
    output->size = byteCount;
    return MakeStatus(SdReadError::None);
}

SdReaderStatus SdFileReader::ReadPrefix(const char* path, void* output,
                                        std::size_t byteCount,
                                        std::size_t maximumSize,
                                        std::size_t* totalSize) {
    if (output == nullptr || path == nullptr || byteCount == 0) {
        return MakeStatus(SdReadError::NotInitialized);
    }

    const auto init = Initialize();
    if (init.error != SdReadError::None) {
        return init;
    }

    nn::fs::FileHandle handle{};
    const Result openResult = m_Open(&handle, path, nn::fs::OpenMode_Read);
    if (openResult != 0) {
        return MakeStatus(SdReadError::OpenFailed, openResult);
    }

    long fileSize = 0;
    const Result sizeResult = m_GetFileSize(&fileSize, handle);
    if (sizeResult != 0) {
        m_Close(handle);
        return MakeStatus(SdReadError::SizeFailed, sizeResult);
    }
    if (fileSize < 0 || static_cast<std::uint64_t>(fileSize) > maximumSize) {
        m_Close(handle);
        return MakeStatus(SdReadError::FileTooLarge);
    }
    if (totalSize != nullptr) {
        *totalSize = static_cast<std::size_t>(fileSize);
    }
    if (static_cast<std::uint64_t>(fileSize) < byteCount) {
        m_Close(handle);
        return MakeStatus(SdReadError::ReadFailed);
    }

    const Result readResult = m_Read(
        handle, 0, output, static_cast<ulong>(byteCount));
    m_Close(handle);
    if (readResult != 0) {
        return MakeStatus(SdReadError::ReadFailed, readResult);
    }
    return MakeStatus(SdReadError::None);
}

const char* SdReadErrorName(SdReadError error) {
    switch (error) {
        case SdReadError::None: return "none";
        case SdReadError::NotInitialized: return "not-initialized";
        case SdReadError::MissingSymbol: return "missing-symbol";
        case SdReadError::OpenFailed: return "open-failed";
        case SdReadError::SizeFailed: return "size-failed";
        case SdReadError::FileTooLarge: return "file-too-large";
        case SdReadError::AllocationFailed: return "allocation-failed";
        case SdReadError::ReadFailed: return "read-failed";
        case SdReadError::NoDirectory: return "no-directory";
        case SdReadError::MultipleDirectories: return "multiple-directories";
        default: return "unknown";
    }
}

} // namespace silkmodloader::skin
