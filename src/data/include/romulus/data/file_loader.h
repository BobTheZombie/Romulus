#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "romulus/data/binary_reader.h"

namespace romulus::data {

enum class FileLoadErrorCode {
  NotFound,
  NotRegularFile,
  FileTooLarge,
  ReadFailed,
};

struct FileLoadError {
  FileLoadErrorCode code = FileLoadErrorCode::ReadFailed;
  std::filesystem::path path;
  std::uintmax_t file_size_bytes = 0;
  std::size_t max_allowed_bytes = 0;
  std::string message;
};

struct LoadedFile {
  std::filesystem::path path;
  std::vector<std::uint8_t> bytes;
};

struct LoadFileResult {
  std::optional<LoadedFile> value;
  std::optional<FileLoadError> error;

  [[nodiscard]] bool ok() const {
    return value.has_value();
  }
};

constexpr std::size_t k_default_max_file_load_bytes = 32 * 1024 * 1024;

[[nodiscard]] LoadFileResult load_file_to_memory(
    const std::filesystem::path& path,
    std::size_t max_allowed_bytes = k_default_max_file_load_bytes);

[[nodiscard]] BinaryReader make_binary_reader(const LoadedFile& file);

}  // namespace romulus::data
