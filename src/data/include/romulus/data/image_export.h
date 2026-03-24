#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "romulus/data/indexed_image.h"

namespace romulus::data {

enum class ImageExportErrorCode {
  InvalidImage,
  IoFailure,
};

struct ImageExportError {
  ImageExportErrorCode code = ImageExportErrorCode::IoFailure;
  std::filesystem::path output_path;
  std::string message;
};

struct ImageExportResult {
  std::optional<ImageExportError> error;

  [[nodiscard]] bool ok() const {
    return !error.has_value();
  }
};

[[nodiscard]] ImageExportResult export_rgba_image_as_ppm(const RgbaImage& image,
                                                          const std::filesystem::path& output_path);

}  // namespace romulus::data
