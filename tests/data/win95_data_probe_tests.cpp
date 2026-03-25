#include "romulus/data/win95_data_probe.h"

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

std::filesystem::path make_temp_test_dir() {
  const auto unique = std::to_string(std::rand()) + "_" + std::to_string(std::rand());
  return std::filesystem::temp_directory_path() / ("romulus_win95_data_probe_tests_" + unique);
}

void write_file(const std::filesystem::path& path, const std::string& bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  output << bytes;
}

int test_file_target_reconnaissance() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir);

  write_file(temp_dir / "DATA", "PACK");
  std::string mz_bytes(64, '\0');
  mz_bytes[0] = 'M';
  mz_bytes[1] = 'Z';
  write_file(temp_dir / "DATA0", mz_bytes);

  const auto result = romulus::data::probe_win95_data_entries(temp_dir);
  if (assert_true(result.ok(), "file targets should probe successfully") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto& report = result.value.value();
  if (assert_true(report.data_entry.node_kind == romulus::data::Win95ProbeNodeKind::File,
                  "DATA should report file node kind") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(report.data_entry.file.has_value() && report.data_entry.file->kind == romulus::data::Win95ProbeFileKind::PossibleContainer,
                  "DATA file should classify as possible container") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(report.data0_entry.file.has_value() && report.data0_entry.file->kind == romulus::data::Win95ProbeFileKind::ExecutableLike,
                  "MZ header should classify DATA0 as executable-like") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

int test_directory_target_reconnaissance_and_sorting() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir / "DATA");
  std::filesystem::create_directories(temp_dir / "DATA0");

  write_file(temp_dir / "DATA" / "ZZZ.BIN", "\x01\x02\x03\x04");
  write_file(temp_dir / "DATA" / "AAA.TXT", "hello\n");
  write_file(temp_dir / "DATA0" / "ONE.DAT", "abcd");

  const auto result = romulus::data::probe_win95_data_entries(temp_dir);
  if (assert_true(result.ok(), "directory targets should probe successfully") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto& data_dir = result.value->data_entry.directory;
  if (assert_true(data_dir.has_value(), "DATA should include directory details") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(data_dir->preview_entries.size() == 2, "directory preview should include bounded entries") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(data_dir->preview_entries[0].name == "AAA.TXT" && data_dir->preview_entries[1].name == "ZZZ.BIN",
                  "directory preview entries should be deterministic and sorted") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

int test_pair_summary_behavior() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir / "DATA");
  write_file(temp_dir / "DATA0", "ABCD1234");

  const auto result = romulus::data::probe_win95_data_entries(temp_dir);
  if (assert_true(result.ok(), "mixed kind targets should still probe") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(result.value->pair_summary.relationship == "different-kind",
                  "pair summary should report different kind") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

int test_missing_target_handling() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir);
  write_file(temp_dir / "DATA", "abc");

  const auto result = romulus::data::probe_win95_data_entries(temp_dir);
  if (assert_true(result.ok(), "missing DATA0 should not fail probe") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(result.value->data0_entry.node_kind == romulus::data::Win95ProbeNodeKind::Missing,
                  "missing DATA0 should be reported explicitly") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto formatted = romulus::data::format_win95_data_probe_report(result.value.value());
  if (assert_true(formatted.find("[pair]") != std::string::npos, "formatted report should include pair section") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

}  // namespace

int main() {
  std::srand(2028);

  if (test_file_target_reconnaissance() != 0) {
    return EXIT_FAILURE;
  }

  if (test_directory_target_reconnaissance_and_sorting() != 0) {
    return EXIT_FAILURE;
  }

  if (test_pair_summary_behavior() != 0) {
    return EXIT_FAILURE;
  }

  if (test_missing_target_handling() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
