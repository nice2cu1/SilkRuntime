#include "png_loader.hpp"

#include <cstdlib>
#include <limits>

#include <png.h>

namespace silkmodloader::image {
namespace {

constexpr std::uint64_t kMaxPixels = 8192ULL * 8192ULL;

bool BeginRead(const void* data, std::size_t size, png_image* image) {
    if (data == nullptr || size == 0 || image == nullptr ||
        size > std::numeric_limits<png_alloc_size_t>::max()) {
        return false;
    }

    *image = {};
    image->version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(image, data,
                                          static_cast<png_alloc_size_t>(size))) {
        png_image_free(image);
        return false;
    }

    if (image->width == 0 || image->height == 0 ||
        static_cast<std::uint64_t>(image->width) * image->height > kMaxPixels) {
        png_image_free(image);
        return false;
    }
    return true;
}

} // namespace

bool InspectPng(const void* data, std::size_t size, PngInfo* output) {
    if (output == nullptr) {
        return false;
    }
    *output = {};

    png_image image{};
    if (!BeginRead(data, size, &image)) {
        return false;
    }

    output->width = image.width;
    output->height = image.height;
    png_image_free(&image);
    return true;
}

bool DecodePngRgba8(const void* data, std::size_t size, DecodedImage* output) {
    if (output == nullptr) {
        return false;
    }
    *output = {};

    png_image image{};
    if (!BeginRead(data, size, &image)) {
        return false;
    }

    const std::uint64_t byteCount = static_cast<std::uint64_t>(image.width) *
                                    image.height * 4ULL;
    if (byteCount > std::numeric_limits<std::size_t>::max()) {
        png_image_free(&image);
        return false;
    }

    auto* rgba = static_cast<std::uint8_t*>(std::malloc(static_cast<std::size_t>(byteCount)));
    if (rgba == nullptr) {
        png_image_free(&image);
        return false;
    }

    image.format = PNG_FORMAT_RGBA;
    if (!png_image_finish_read(&image, nullptr, rgba, 0, nullptr)) {
        std::free(rgba);
        png_image_free(&image);
        return false;
    }

    output->width = image.width;
    output->height = image.height;
    output->rgba = rgba;
    output->byteSize = static_cast<std::size_t>(byteCount);
    png_image_free(&image);
    return true;
}

void ReleaseDecodedImage(DecodedImage* image) {
    if (image == nullptr) {
        return;
    }
    std::free(image->rgba);
    *image = {};
}

} // namespace silkmodloader::image
