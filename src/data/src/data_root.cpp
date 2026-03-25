#include "romulus/data/data_root.h"

#include <sstream>

namespace romulus::data {
namespace {

const std::vector<RequiredEntry> kRequiredEntries = {
    {.relative_path = "CAESAR2.EXE", .type = RequiredEntryType::File},
    {.relative_path = "CAESAR2.INI", .type = RequiredEntryType::File},
    {.relative_path = "DATA", .type = RequiredEntryType::Directory},
    {.relative_path = "SAVE", .type = RequiredEntryType::Directory},
    {.relative_path = "HISTORY.DAT", .type = RequiredEntryType::File},
};

const std::vector<RequiredEntry> kExpectedOptionalEntries = {
    {.relative_path = "DATA0", .type = RequiredEntryType::Directory},
    {.relative_path = "RAW", .type = RequiredEntryType::Directory},
    {.relative_path = "SMK", .type = RequiredEntryType::Directory},
    {.relative_path = "CAESAR2.INF", .type = RequiredEntryType::File},
};

[[nodiscard]] bool entry_exists_with_type(const std::filesystem::path& root, const RequiredEntry& entry) {
  const std::filesystem::path candidate = root / entry.relative_path;
  std::error_code error;

  if (entry.type == RequiredEntryType::File) {
    return std::filesystem::is_regular_file(candidate, error);
  }

  return std::filesystem::is_directory(candidate, error);
}

[[nodiscard]] std::string entry_type_to_string(RequiredEntryType type) {
  return type == RequiredEntryType::File ? "file" : "directory";
}

}  // namespace

const std::vector<RequiredEntry>& required_entries() {
  return kRequiredEntries;
}

const std::vector<RequiredEntry>& expected_optional_entries() {
  return kExpectedOptionalEntries;
}

std::filesystem::path resolve_data_root(std::string_view data_root_argument) {
  std::filesystem::path root(data_root_argument);

  if (root.is_relative()) {
    root = std::filesystem::current_path() / root;
  }

  return root.lexically_normal();
}

DataRootValidationResult validate_data_root(const std::filesystem::path& data_root) {
  DataRootValidationResult result;
  result.root = data_root;

  std::error_code error;
  if (!std::filesystem::exists(data_root, error)) {
    return result;
  }

  if (!std::filesystem::is_directory(data_root, error)) {
    return result;
  }

  for (const RequiredEntry& entry : kRequiredEntries) {
    if (!entry_exists_with_type(data_root, entry)) {
      result.missing_entries.push_back(entry);
    }
  }

  result.ok = result.missing_entries.empty();
  return result;
}

std::string format_validation_error(const DataRootValidationResult& validation) {
  std::ostringstream message;
  message << "Invalid Caesar II data root: '" << validation.root.string() << "'. ";

  std::error_code error;
  if (!std::filesystem::exists(validation.root, error)) {
    message << "Path does not exist.";
    return message.str();
  }

  if (!std::filesystem::is_directory(validation.root, error)) {
    message << "Path is not a directory.";
    return message.str();
  }

  message << "Missing required Caesar II data entries:";
  for (const RequiredEntry& entry : validation.missing_entries) {
    message << "\n  - " << entry.relative_path.string() << " (" << entry_type_to_string(entry.type) << ")";
  }

  return message.str();
}

}  // namespace romulus::data
