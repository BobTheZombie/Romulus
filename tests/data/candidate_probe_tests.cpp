#include "romulus/data/candidate_probe.h"

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
  return std::filesystem::temp_directory_path() / ("romulus_candidate_probe_tests_" + unique);
}

int test_probe_reports_text_like_cfg_with_preview() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir);

  {
    std::ofstream output(temp_dir / "RESOURCE.CFG", std::ios::binary);
    output << "KEY=VALUE\nANOTHER=ENTRY\n";
  }

  const auto result = romulus::data::probe_candidate_files(temp_dir, {"RESOURCE.CFG"});
  if (assert_true(result.ok(), "candidate probe should load text cfg") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto& report = result.value.value();
  if (assert_true(report.files.size() == 1, "probe should return single file report") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto& file = report.files[0];
  if (assert_true(file.kind == romulus::data::CandidateFileKind::TextLike,
                  "RESOURCE.CFG should classify as text-like") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(file.text_preview.has_value(), "text-like file should include preview") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(file.text_preview->text.find("KEY=VALUE") != std::string::npos,
                  "text preview should include leading config content") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

int test_probe_reports_mz_executable_like_kind() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir);

  {
    std::ofstream output(temp_dir / "EXEC.DAT", std::ios::binary);
    std::string bytes(64, '\0');
    bytes[0] = 'M';
    bytes[1] = 'Z';
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  }

  const auto result = romulus::data::probe_candidate_files(temp_dir, {"EXEC.DAT"});
  if (assert_true(result.ok(), "candidate probe should load MZ-like file") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto& file = result.value.value().files[0];
  if (assert_true(file.kind == romulus::data::CandidateFileKind::MzExecutableLike,
                  "MZ signature should classify as executable-like") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

int test_probe_reports_dat_ix_pair() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir);

  {
    std::ofstream dat(temp_dir / "DISCS.DAT", std::ios::binary);
    dat.write("ABCD", 4);
  }

  {
    std::ofstream ix(temp_dir / "DISCS.IX", std::ios::binary);
    ix.write("1234", 4);
  }

  const auto result = romulus::data::probe_candidate_files(temp_dir, {"DISCS.DAT", "DISCS.IX"});
  if (assert_true(result.ok(), "candidate probe should load DAT/IX pair") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto& pairs = result.value.value().dat_ix_pairs;
  if (assert_true(pairs.size() == 1, "probe should report one DAT/IX pair") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(pairs[0].dat_relative_path.generic_string() == "DISCS.DAT",
                  "pair should preserve DAT path") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(pairs[0].ix_relative_path.generic_string() == "DISCS.IX", "pair should preserve IX path") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto formatted = romulus::data::format_candidate_probe_report(result.value.value());
  if (assert_true(formatted.find("[dat_ix_pairs]") != std::string::npos,
                  "formatted report should include DAT/IX section") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

int test_probe_is_bounded_by_max_load_size() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir);

  {
    std::ofstream output(temp_dir / "HISTORY.DAT", std::ios::binary);
    output.write("123456", 6);
  }

  const auto result = romulus::data::probe_candidate_files(temp_dir, {"HISTORY.DAT"}, 5);
  if (assert_true(!result.ok(), "oversized candidate should fail bounded probe") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(result.error.has_value(), "bounded probe failure should include error") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

}  // namespace

int main() {
  std::srand(1815);

  if (test_probe_reports_text_like_cfg_with_preview() != 0) {
    return EXIT_FAILURE;
  }

  if (test_probe_reports_mz_executable_like_kind() != 0) {
    return EXIT_FAILURE;
  }

  if (test_probe_reports_dat_ix_pair() != 0) {
    return EXIT_FAILURE;
  }

  if (test_probe_is_bounded_by_max_load_size() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
