#pragma once

#include "replacement_database.hpp"
#include "sd_file_reader.hpp"

#include <string_view>

namespace silkmodloader::skin {

class SkinScanner final {
public:
    void BindVerifiedMain(std::uintptr_t mainBase);

    SdReaderStatus FindSingleSkinRoot(const char* parentPath,
                                      char* outputPath,
                                      std::size_t outputCapacity);

    /* Resolve files directly from the directory convention.  The runtime
     * deliberately does not require a skin manifest: collection names and
     * Unity texture names are the keys, while the filename encoding keeps
     * those keys safe on the SD filesystem. */
    SdReaderStatus ResolveCollection(const char* skinRoot,
                                     std::string_view collection,
                                     std::uint32_t atlasIndex,
                                     ReplacementTexture* output);
    SdReaderStatus ResolveStandalone(const char* skinRoot,
                                     std::string_view textureName,
                                     ReplacementTexture* output);
    SdReaderStatus ResolveSpriteTexture(const char* skinRoot,
                                       std::string_view textureName,
                                       ReplacementTexture* output);

    static bool EncodeResourceFilename(std::string_view resource,
                                       char* output, std::size_t capacity);

    SdReaderStatus ReadReplacement(const ReplacementTexture& replacement,
                                   FileBuffer* output);

private:
    static bool JoinPath(char* output, std::size_t capacity,
                         const char* root, const char* suffix);
    SdReaderStatus ResolveNamed(const char* skinRoot,
                                std::string_view directory,
                                std::string_view resource,
                                ReplacementKind kind,
                                ReplacementTexture* output);

    SdFileReader m_Reader;
};

} // namespace silkmodloader::skin
