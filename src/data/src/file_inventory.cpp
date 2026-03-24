#include "romulus/data/file_inventory.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

#include "romulus/data/data_root.h"

namespace romulus::data {
namespace {

[[nodiscard]] std::unordered_set<std::string> required_file_paths() {
  std::unordered_set<std::string> required;
  for (const RequiredEntry& entry : required_entries()) {
    if (entry.type != RequiredEntryType::File) {
      continue;
    }

    required.insert(entry.relative_path.lexically_normal().generic_string());
  }

  return required;
}

}  // namespace

FileInventoryManifest build_file_inventory(const std::filesystem::path& validated_data_root) {
  FileInventoryManifest manifest;
  manifest.root = validated_data_root;

  const auto required = required_file_paths();

  std::error_code error;
  for (std::filesystem::recursive_directory_iterator iterator(validated_data_root, error), end; iterator != end;
       iterator.increment(error)) {
    if (error) {
      break;
    }

    const auto& candidate = iterator->path();
    if (!iterator->is_regular_file(error) || error) {
      continue;
    }

    const auto relative = candidate.lexically_relative(validated_data_root).lexically_normal();
    const auto relative_string = relative.generic_string();

    const auto size_bytes = std::filesystem::file_size(candidate, error);
    if (error) {
      continue;
    }

    FileInventoryEntry entry;
    entry.filename = candidate.filename().string();
    entry.relative_path = relative;
    entry.size_bytes = size_bytes;
    entry.is_required_known_entry = required.contains(relative_string);

    if (entry.is_required_known_entry) {
      ++manifest.required_known_file_count;
    } else {
      ++manifest.other_file_count;
    }

    manifest.entries.push_back(std::move(entry));
  }

  std::sort(manifest.entries.begin(), manifest.entries.end(), [](const FileInventoryEntry& lhs, const FileInventoryEntry& rhs) {
    return lhs.relative_path.generic_string() < rhs.relative_path.generic_string();
  });

  return manifest;
}

std::string format_file_inventory_manifest(const FileInventoryManifest& manifest) {
  std::ostringstream output;
  output << "# Caesar II File Manifest\n";
  output << "root: " << manifest.root.string() << "\n";
  output << "total_files: " << manifest.entries.size() << "\n";
  output << "required_known_files: " << manifest.required_known_file_count << "\n";
  output << "other_files: " << manifest.other_file_count << "\n\n";
  output << "classification|relative_path|filename|size_bytes\n";

  for (const auto& entry : manifest.entries) {
    output << (entry.is_required_known_entry ? "required" : "discovered") << '|';
    output << entry.relative_path.generic_string() << '|';
    output << entry.filename << '|';
    output << entry.size_bytes << '\n';
  }

  return output.str();
}

}  // namespace romulus::data
