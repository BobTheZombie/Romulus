#include "romulus/platform/bootstrap.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int assert_true(const bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    return 1;
  }
  return 0;
}

std::filesystem::path make_temp_dir(const std::string& suffix) {
  const auto root = std::filesystem::temp_directory_path();
  const auto unique_name = "romulus-bootstrap-tests-" + suffix + "-" + std::to_string(std::rand());
  const auto path = root / unique_name;
  std::filesystem::create_directories(path);
  return path;
}

void write_file(const std::filesystem::path& path) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path);
  stream << "test";
}

int test_select_bootstrap_asset_prefers_forum_lbm() {
  const auto root = make_temp_dir("prefers-forum");
  write_file(root / "data" / "forum.lbm");
  write_file(root / "data" / "empire2.lbm");

  const auto selected = romulus::platform::select_bootstrap_asset(root);
  const int rc = assert_true(selected.has_value() && selected->logical_path == std::filesystem::path("data/forum.lbm"),
                             "bootstrap should prefer data/forum.lbm");

  std::filesystem::remove_all(root);
  return rc;
}

int test_select_bootstrap_asset_falls_back_when_forum_missing() {
  const auto root = make_temp_dir("fallback-empire2");
  write_file(root / "data" / "empire2.lbm");

  const auto selected = romulus::platform::select_bootstrap_asset(root);
  const int rc =
      assert_true(selected.has_value() && selected->logical_path == std::filesystem::path("data/empire2.lbm"),
                  "bootstrap should fall back to data/empire2.lbm");

  std::filesystem::remove_all(root);
  return rc;
}

int test_select_bootstrap_asset_honors_override() {
  const auto root = make_temp_dir("override");
  write_file(root / "custom" / "startup.lbm");
  write_file(root / "data" / "forum.lbm");

  const auto selected = romulus::platform::select_bootstrap_asset(root, std::filesystem::path("custom/startup.lbm"));
  const int rc = assert_true(selected.has_value() && selected->used_override &&
                                 selected->logical_path == std::filesystem::path("custom/startup.lbm"),
                             "bootstrap should use override path when present");

  std::filesystem::remove_all(root);
  return rc;
}

int test_select_bootstrap_asset_returns_empty_when_no_candidates_exist() {
  const auto root = make_temp_dir("missing-all");
  const auto selected = romulus::platform::select_bootstrap_asset(root);
  const int rc = assert_true(!selected.has_value(), "bootstrap selection should fail cleanly when no files exist");

  std::filesystem::remove_all(root);
  return rc;
}

}  // namespace

int main() {
  if (test_select_bootstrap_asset_prefers_forum_lbm() != 0) {
    return EXIT_FAILURE;
  }

  if (test_select_bootstrap_asset_falls_back_when_forum_missing() != 0) {
    return EXIT_FAILURE;
  }

  if (test_select_bootstrap_asset_honors_override() != 0) {
    return EXIT_FAILURE;
  }

  if (test_select_bootstrap_asset_returns_empty_when_no_candidates_exist() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
