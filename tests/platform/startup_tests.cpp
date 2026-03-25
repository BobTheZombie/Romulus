#include "romulus/platform/startup.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "romulus/data/data_root.h"

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
  const auto unique_name = "romulus-startup-tests-" + suffix + "-" + std::to_string(std::rand());
  const auto path = root / unique_name;
  std::filesystem::create_directories(path);
  return path;
}

void write_file(const std::filesystem::path& path) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path);
  stream << "test";
}

void create_valid_win95_layout(const std::filesystem::path& root) {
  for (const auto& entry : romulus::data::required_entries()) {
    const auto target = root / entry.relative_path;
    if (entry.type == romulus::data::RequiredEntryType::Directory) {
      std::filesystem::create_directories(target);
    } else {
      write_file(target);
    }
  }
}

int test_startup_without_data_root_enters_setup_state() {
  const auto status = romulus::platform::evaluate_startup_data_root(std::nullopt);
  return assert_true(status.state == romulus::platform::StartupState::NoDataRootConfigured,
                     "missing configured root should enter setup state");
}

int test_invalid_selected_root_stays_invalid() {
  const auto root = make_temp_dir("invalid-selected-root");
  const auto status = romulus::platform::evaluate_startup_data_root(root);

  const int result = assert_true(status.state == romulus::platform::StartupState::DataRootInvalid,
                                 "missing required files should keep startup invalid");
  std::filesystem::remove_all(root);
  return result;
}

int test_valid_selected_root_transitions_to_ready() {
  const auto root = make_temp_dir("valid-selected-root");
  create_valid_win95_layout(root);

  const auto status = romulus::platform::evaluate_startup_data_root(root);
  const int result = assert_true(status.state == romulus::platform::StartupState::DataRootReady,
                                 "valid selected root should be ready");

  std::filesystem::remove_all(root);
  return result;
}

int test_persisted_root_reused_on_next_launch_when_valid() {
  const auto root = make_temp_dir("persisted-root");
  create_valid_win95_layout(root);

  const auto config_dir = make_temp_dir("config");
  const auto config_file = config_dir / "startup.conf";

  if (assert_true(romulus::platform::persist_data_root(config_file, root), "persist should succeed") != 0) {
    std::filesystem::remove_all(root);
    std::filesystem::remove_all(config_dir);
    return 1;
  }

  const auto loaded = romulus::platform::load_persisted_data_root(config_file);
  if (assert_true(loaded.has_value(), "persisted root should be loaded") != 0) {
    std::filesystem::remove_all(root);
    std::filesystem::remove_all(config_dir);
    return 1;
  }

  const auto status = romulus::platform::evaluate_startup_data_root(*loaded);
  const int result = assert_true(status.state == romulus::platform::StartupState::DataRootReady,
                                 "loaded persisted root should still validate");

  std::filesystem::remove_all(root);
  std::filesystem::remove_all(config_dir);
  return result;
}

}  // namespace

int main() {
  if (test_startup_without_data_root_enters_setup_state() != 0) {
    return EXIT_FAILURE;
  }

  if (test_invalid_selected_root_stays_invalid() != 0) {
    return EXIT_FAILURE;
  }

  if (test_valid_selected_root_transitions_to_ready() != 0) {
    return EXIT_FAILURE;
  }

  if (test_persisted_root_reused_on_next_launch_when_valid() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
