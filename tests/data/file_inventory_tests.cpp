#include "romulus/data/file_inventory.h"

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
  const auto unique_name = "romulus-file-inventory-tests-" + suffix + "-" + std::to_string(std::rand());
  const auto path = root / unique_name;
  std::filesystem::create_directories(path);
  return path;
}

void write_file(const std::filesystem::path& path, const std::string& contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path, std::ios::binary);
  stream << contents;
}

int test_manifest_discovers_required_and_other_files() {
  const auto root = make_temp_dir("discover");

  write_file(root / "CAESAR2.EXE", "exe");
  write_file(root / "CAESAR2.INI", "config");
  std::filesystem::create_directories(root / "DATA");
  std::filesystem::create_directories(root / "SAVE");
  write_file(root / "HISTORY.DAT", "history");

  write_file(root / "DATA/EXTRA.TXT", "extra");
  write_file(root / "README.LOCAL", "notes");

  const auto manifest = romulus::data::build_file_inventory(root);
  if (assert_true(manifest.entries.size() == 5, "manifest should include all regular files") != 0) {
    std::filesystem::remove_all(root);
    return 1;
  }

  if (assert_true(manifest.required_known_file_count == 3, "required known file count should include known required files") != 0) {
    std::filesystem::remove_all(root);
    return 1;
  }

  if (assert_true(manifest.other_file_count == 2, "other file count should include discovered files") != 0) {
    std::filesystem::remove_all(root);
    return 1;
  }

  bool found_required_cfg = false;
  bool found_extra_file = false;
  for (const auto& entry : manifest.entries) {
    if (entry.relative_path.generic_string() == "CAESAR2.INI") {
      found_required_cfg = entry.is_required_known_entry && entry.filename == "CAESAR2.INI" && entry.size_bytes == 6;
    }

    if (entry.relative_path.generic_string() == "DATA/EXTRA.TXT") {
      found_extra_file = !entry.is_required_known_entry && entry.filename == "EXTRA.TXT" && entry.size_bytes == 5;
    }
  }

  const int required_status = assert_true(found_required_cfg, "required file entry should include metadata and classification");
  const int extra_status = assert_true(found_extra_file, "discovered file entry should include metadata and classification");

  std::filesystem::remove_all(root);
  return required_status != 0 ? required_status : extra_status;
}

int test_manifest_format_is_stable_and_sorted() {
  const auto root = make_temp_dir("format");

  write_file(root / "CAESAR2.EXE", "exe");
  write_file(root / "CAESAR2.INI", "cfg");
  std::filesystem::create_directories(root / "DATA");
  std::filesystem::create_directories(root / "SAVE");
  write_file(root / "HISTORY.DAT", "his");
  write_file(root / "ZZZ.DAT", "zzz");
  write_file(root / "AAA.DAT", "aaa");

  const auto manifest = romulus::data::build_file_inventory(root);
  const auto text = romulus::data::format_file_inventory_manifest(manifest);

  const auto header_pos = text.find("classification|relative_path|filename|size_bytes");
  if (assert_true(header_pos != std::string::npos, "manifest format should include a table header") != 0) {
    std::filesystem::remove_all(root);
    return 1;
  }

  const auto aaa_pos = text.find("discovered|AAA.DAT|AAA.DAT|3");
  const auto zzz_pos = text.find("discovered|ZZZ.DAT|ZZZ.DAT|3");
  const auto required_pos = text.find("required|CAESAR2.INI|CAESAR2.INI|3");

  if (assert_true(aaa_pos != std::string::npos && zzz_pos != std::string::npos && required_pos != std::string::npos,
                  "manifest text should contain expected rows") != 0) {
    std::filesystem::remove_all(root);
    return 1;
  }

  const int status = assert_true(aaa_pos < zzz_pos, "manifest rows should be sorted by relative path");
  std::filesystem::remove_all(root);
  return status;
}

}  // namespace

int main() {
  if (test_manifest_discovers_required_and_other_files() != 0) {
    return EXIT_FAILURE;
  }

  if (test_manifest_format_is_stable_and_sorted() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
