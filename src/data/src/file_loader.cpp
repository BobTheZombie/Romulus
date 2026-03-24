#include "romulus/data/file_loader.h"

#include <fstream>
#include <sstream>

namespace romulus::data {
namespace {

[[nodiscard]] FileLoadError make_not_found_error(const std::filesystem::path& path) {
  FileLoadError error;
  error.code = FileLoadErrorCode::NotFound;
  error.path = path;
  error.message = "File does not exist: " + path.string();
  return error;
}

[[nodiscard]] FileLoadError make_not_regular_file_error(const std::filesystem::path& path) {
  FileLoadError error;
  error.code = FileLoadErrorCode::NotRegularFile;
  error.path = path;
  error.message = "Path is not a regular file: " + path.string();
  return error;
}

[[nodiscard]] FileLoadError make_file_too_large_error(
    const std::filesystem::path& path,
    const std::uintmax_t file_size_bytes,
    const std::size_t max_allowed_bytes) {
  std::ostringstream message;
  message << "File exceeds max allowed load size: path=" << path.string() << ", file_size_bytes=" << file_size_bytes
          << ", max_allowed_bytes=" << max_allowed_bytes;

  FileLoadError error;
  error.code = FileLoadErrorCode::FileTooLarge;
  error.path = path;
  error.file_size_bytes = file_size_bytes;
  error.max_allowed_bytes = max_allowed_bytes;
  error.message = message.str();
  return error;
}

[[nodiscard]] FileLoadError make_read_failed_error(
    const std::filesystem::path& path,
    const std::uintmax_t file_size_bytes,
    const std::string& reason) {
  std::ostringstream message;
  message << "Failed to read file into memory: path=" << path.string() << ", file_size_bytes=" << file_size_bytes
          << ", reason=" << reason;

  FileLoadError error;
  error.code = FileLoadErrorCode::ReadFailed;
  error.path = path;
  error.file_size_bytes = file_size_bytes;
  error.message = message.str();
  return error;
}

}  // namespace

LoadFileResult load_file_to_memory(const std::filesystem::path& path, const std::size_t max_allowed_bytes) {
  std::error_code error;
  const bool exists = std::filesystem::exists(path, error);
  if (error || !exists) {
    return {.error = make_not_found_error(path)};
  }

  const bool is_regular_file = std::filesystem::is_regular_file(path, error);
  if (error || !is_regular_file) {
    return {.error = make_not_regular_file_error(path)};
  }

  const auto file_size = std::filesystem::file_size(path, error);
  if (error) {
    return {.error = make_read_failed_error(path, 0, "file_size failed")};
  }

  if (file_size > max_allowed_bytes) {
    return {.error = make_file_too_large_error(path, file_size, max_allowed_bytes)};
  }

  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return {.error = make_read_failed_error(path, file_size, "open failed")};
  }

  LoadedFile loaded;
  loaded.path = path;
  loaded.bytes.resize(static_cast<std::size_t>(file_size));

  if (file_size > 0) {
    input.read(reinterpret_cast<char*>(loaded.bytes.data()), static_cast<std::streamsize>(file_size));
    if (!input || input.gcount() != static_cast<std::streamsize>(file_size)) {
      return {.error = make_read_failed_error(path, file_size, "short read")};
    }
  }

  return {.value = std::move(loaded)};
}

BinaryReader make_binary_reader(const LoadedFile& file) {
  return BinaryReader(std::span<const std::uint8_t>(file.bytes.data(), file.bytes.size()));
}

}  // namespace romulus::data
