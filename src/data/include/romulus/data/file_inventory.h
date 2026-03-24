#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace romulus::data {

struct FileInventoryEntry {
  std::string filename;
  std::filesystem::path relative_path;
  std::uintmax_t size_bytes = 0;
  bool is_required_known_entry = false;
};

struct FileInventoryManifest {
  std::filesystem::path root;
  std::vector<FileInventoryEntry> entries;
  std::size_t required_known_file_count = 0;
  std::size_t other_file_count = 0;
};

[[nodiscard]] FileInventoryManifest build_file_inventory(const std::filesystem::path& validated_data_root);
[[nodiscard]] std::string format_file_inventory_manifest(const FileInventoryManifest& manifest);

}  // namespace romulus::data
