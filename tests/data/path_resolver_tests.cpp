#include "romulus/data/path_resolver.h"

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
  const auto unique_name = "romulus-path-resolver-tests-" + suffix + "-" + std::to_string(std::rand());
  const auto path = root / unique_name;
  std::filesystem::create_directories(path);
  return path;
}

void write_file(const std::filesystem::path& path) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path);
  stream << "test";
}

int test_resolves_exact_case_match() {
  const auto root = make_temp_dir("exact");
  write_file(root / "DATA" / "FORUM.LBM");

  const auto resolved = romulus::data::resolve_case_insensitive(root, "DATA/FORUM.LBM");
  const int rc = assert_true(resolved.has_value() && resolved->filename() == "FORUM.LBM",
                             "resolver should preserve exact filename casing");

  std::filesystem::remove_all(root);
  return rc;
}

int test_resolves_case_insensitive_directory_match() {
  const auto root = make_temp_dir("dir-case");
  write_file(root / "DATA" / "FORUM.LBM");

  const auto resolved = romulus::data::resolve_case_insensitive(root, "data/FORUM.LBM");
  const int rc = assert_true(resolved.has_value() && resolved->parent_path().filename() == "DATA",
                             "resolver should match directory case-insensitively and preserve on-disk case");

  std::filesystem::remove_all(root);
  return rc;
}

int test_resolves_case_insensitive_filename_match() {
  const auto root = make_temp_dir("file-case");
  write_file(root / "DATA" / "FORUM.LBM");

  const auto resolved = romulus::data::resolve_case_insensitive(root, "DATA/forum.lbm");
  const int rc = assert_true(resolved.has_value() && resolved->filename() == "FORUM.LBM",
                             "resolver should match file name case-insensitively");

  std::filesystem::remove_all(root);
  return rc;
}

int test_resolves_mixed_case_input() {
  const auto root = make_temp_dir("mixed");
  write_file(root / "DaTa" / "FoRuM.LbM");

  const auto resolved = romulus::data::resolve_case_insensitive(root, "dAtA/fOrUm.lBm");
  const int rc = assert_true(resolved.has_value() && resolved->filename() == "FoRuM.LbM" &&
                                 resolved->parent_path().filename() == "DaTa",
                             "resolver should handle mixed-case input across all segments");

  std::filesystem::remove_all(root);
  return rc;
}

int test_supports_backslash_and_forward_slash_paths() {
  const auto root = make_temp_dir("slashes");
  write_file(root / "DATA" / "FORUM.LBM");

  const auto resolved_backslash = romulus::data::resolve_case_insensitive(root, "data\\forum.lbm");
  const auto resolved_forward = romulus::data::resolve_case_insensitive(root, "data/forum.lbm");
  const int rc = assert_true(resolved_backslash.has_value() && resolved_forward.has_value() &&
                                 *resolved_backslash == *resolved_forward,
                             "resolver should normalize backslashes and forward slashes");

  std::filesystem::remove_all(root);
  return rc;
}

int test_returns_empty_for_not_found_path() {
  const auto root = make_temp_dir("missing");
  write_file(root / "DATA" / "FORUM.LBM");

  const auto resolved = romulus::data::resolve_case_insensitive(root, "data/empire2.lbm");
  const int rc = assert_true(!resolved.has_value(), "resolver should return empty when path is missing");

  std::filesystem::remove_all(root);
  return rc;
}

int test_rejects_path_traversal() {
  const auto root = make_temp_dir("traversal");
  write_file(root / "DATA" / "FORUM.LBM");

  const auto resolved_parent = romulus::data::resolve_case_insensitive(root, "../DATA/FORUM.LBM");
  const auto resolved_dot = romulus::data::resolve_case_insensitive(root, "./DATA/FORUM.LBM");
  const int rc = assert_true(!resolved_parent.has_value() && !resolved_dot.has_value(),
                             "resolver should reject traversal or malformed relative segments");

  std::filesystem::remove_all(root);
  return rc;
}

}  // namespace

int main() {
  if (test_resolves_exact_case_match() != 0) {
    return EXIT_FAILURE;
  }

  if (test_resolves_case_insensitive_directory_match() != 0) {
    return EXIT_FAILURE;
  }

  if (test_resolves_case_insensitive_filename_match() != 0) {
    return EXIT_FAILURE;
  }

  if (test_resolves_mixed_case_input() != 0) {
    return EXIT_FAILURE;
  }

  if (test_supports_backslash_and_forward_slash_paths() != 0) {
    return EXIT_FAILURE;
  }

  if (test_returns_empty_for_not_found_path() != 0) {
    return EXIT_FAILURE;
  }

  if (test_rejects_path_traversal() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
