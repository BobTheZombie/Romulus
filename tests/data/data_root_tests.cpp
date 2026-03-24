#include "romulus/data/data_root.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int assert_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    return 1;
  }

  return 0;
}

std::filesystem::path make_temp_dir(const std::string& suffix) {
  const auto root = std::filesystem::temp_directory_path();
  const auto unique_name = "romulus-data-root-tests-" + suffix + "-" + std::to_string(std::rand());
  const auto path = root / unique_name;
  std::filesystem::create_directories(path);
  return path;
}

void write_file(const std::filesystem::path& path) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path);
  stream << "test";
}

int test_relative_data_root_resolution() {
  const auto cwd = std::filesystem::current_path();
  const auto resolved = romulus::data::resolve_data_root("./assets/../assets/data");
  const auto expected = (cwd / "assets/data").lexically_normal();
  return assert_true(resolved == expected, "relative paths should resolve from cwd and normalize");
}

int test_missing_path_error_message() {
  const auto missing = std::filesystem::temp_directory_path() / "romulus-data-root-tests-does-not-exist";
  const auto validation = romulus::data::validate_data_root(missing);
  if (assert_true(!validation.ok, "nonexistent root should fail validation") != 0) {
    return 1;
  }

  const auto message = romulus::data::format_validation_error(validation);
  return assert_true(message.find("does not exist") != std::string::npos,
                     "missing path message should mention that the path does not exist");
}

int test_missing_required_files_are_reported() {
  const auto root = make_temp_dir("missing-files");

  const auto& required = romulus::data::required_entries();
  if (required.empty()) {
    return assert_true(false, "required entries list must not be empty");
  }

  const auto first = required.front();
  for (std::size_t index = 1; index < required.size(); ++index) {
    const auto& entry = required[index];
    const auto target = root / entry.relative_path;
    if (entry.type == romulus::data::RequiredEntryType::Directory) {
      std::filesystem::create_directories(target);
    } else {
      write_file(target);
    }
  }

  const auto validation = romulus::data::validate_data_root(root);
  if (assert_true(!validation.ok, "validation should fail when one required entry is missing") != 0) {
    std::filesystem::remove_all(root);
    return 1;
  }

  const auto message = romulus::data::format_validation_error(validation);
  const bool mentions_missing = message.find(first.relative_path.string()) != std::string::npos;
  const int status = assert_true(mentions_missing, "error message should list missing relative path");

  std::filesystem::remove_all(root);
  return status;
}

int test_valid_root_passes_validation() {
  const auto root = make_temp_dir("valid");
  for (const auto& entry : romulus::data::required_entries()) {
    const auto target = root / entry.relative_path;
    if (entry.type == romulus::data::RequiredEntryType::Directory) {
      std::filesystem::create_directories(target);
    } else {
      write_file(target);
    }
  }

  const auto validation = romulus::data::validate_data_root(root);
  const int status = assert_true(validation.ok, "validation should pass when all required entries exist");

  std::filesystem::remove_all(root);
  return status;
}

}  // namespace

int main() {
  if (test_relative_data_root_resolution() != 0) {
    return EXIT_FAILURE;
  }

  if (test_missing_path_error_message() != 0) {
    return EXIT_FAILURE;
  }

  if (test_missing_required_files_are_reported() != 0) {
    return EXIT_FAILURE;
  }

  if (test_valid_root_passes_validation() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
