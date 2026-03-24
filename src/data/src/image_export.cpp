#include "romulus/data/image_export.h"

#include <fstream>
#include <sstream>

namespace romulus::data {
namespace {

[[nodiscard]] ImageExportError make_invalid_image_error(const std::filesystem::path& output_path,
                                                        const std::string& message) {
  return ImageExportError{
      .code = ImageExportErrorCode::InvalidImage,
      .output_path = output_path,
      .message = message,
  };
}

[[nodiscard]] ImageExportError make_io_error(const std::filesystem::path& output_path, const std::string& message) {
  return ImageExportError{
      .code = ImageExportErrorCode::IoFailure,
      .output_path = output_path,
      .message = message,
  };
}

}  // namespace

ImageExportResult export_rgba_image_as_ppm(const RgbaImage& image, const std::filesystem::path& output_path) {
  if (image.width == 0 || image.height == 0) {
    return {.error = make_invalid_image_error(output_path, "RGBA image dimensions must be non-zero")};
  }

  const auto expected_rgba_bytes = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4;
  if (image.pixels_rgba.size() != expected_rgba_bytes) {
    std::ostringstream message;
    message << "RGBA buffer size does not match image dimensions: expected=" << expected_rgba_bytes
            << ", actual=" << image.pixels_rgba.size();
    return {.error = make_invalid_image_error(output_path, message.str())};
  }

  std::ofstream stream(output_path, std::ios::binary);
  if (!stream.is_open()) {
    return {.error = make_io_error(output_path, "Failed to open output file for writing")};
  }

  stream << "P6\n" << image.width << ' ' << image.height << "\n255\n";
  if (!stream.good()) {
    return {.error = make_io_error(output_path, "Failed to write PPM header")};
  }

  for (std::size_t offset = 0; offset < image.pixels_rgba.size(); offset += 4) {
    const char rgb[3] = {
        static_cast<char>(image.pixels_rgba[offset]),
        static_cast<char>(image.pixels_rgba[offset + 1]),
        static_cast<char>(image.pixels_rgba[offset + 2]),
    };

    stream.write(rgb, sizeof(rgb));
    if (!stream.good()) {
      return {.error = make_io_error(output_path, "Failed to write PPM pixel payload")};
    }
  }

  stream.close();
  if (!stream.good()) {
    return {.error = make_io_error(output_path, "Failed to finalize PPM output")};
  }

  return {};
}

}  // namespace romulus::data
