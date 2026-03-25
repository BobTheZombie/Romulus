#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace romulus::data {

enum class RequiredEntryType {
  File,
  Directory,
};

struct RequiredEntry {
  std::filesystem::path relative_path;
  RequiredEntryType type = RequiredEntryType::File;
};

struct DataRootValidationResult {
  bool ok = false;
  std::filesystem::path root;
  std::vector<RequiredEntry> missing_entries;
};

[[nodiscard]] const std::vector<RequiredEntry>& required_entries();
[[nodiscard]] const std::vector<RequiredEntry>& expected_optional_entries();
[[nodiscard]] std::filesystem::path resolve_data_root(std::string_view data_root_argument);
[[nodiscard]] DataRootValidationResult validate_data_root(const std::filesystem::path& data_root);
[[nodiscard]] std::string format_validation_error(const DataRootValidationResult& validation);

}  // namespace romulus::data
