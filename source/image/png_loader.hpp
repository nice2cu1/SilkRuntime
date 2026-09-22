#pragma once

#include <cstddef>
#include <cstdint>

namespace silkmodloader::image {

struct PngInfo {
    std::uint32_t width{};
    std::uint32_t height{};
};

struct DecodedImage {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint8_t* rgba{};
    std::size_t byteSize{};
};

/* The native decoder is an offline/diagnostic helper. The runtime replacement
 * path passes the original PNG bytes to Unity's verified LoadImage binding. */
bool InspectPng(const void* data, std::size_t size, PngInfo* output);
bool DecodePngRgba8(const void* data, std::size_t size, DecodedImage* output);
void ReleaseDecodedImage(DecodedImage* image);

} // namespace silkmodloader::image
