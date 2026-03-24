#include "romulus/data/binary_probe.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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
  return std::filesystem::temp_directory_path() / ("romulus_binary_probe_tests_" + unique);
}

int test_probe_reports_basic_scalars() {
  romulus::data::LoadedFile file;
  file.path = "memory.bin";
  file.bytes = {0x10, 0x32, 0x54, 0x76};

  const auto report = romulus::data::probe_loaded_binary(file);
  if (assert_true(report.size_bytes == 4, "probe should report byte size") != 0) {
    return 1;
  }

  if (assert_true(report.signature_hex == "10 32 54 76", "probe should show hex signature") != 0) {
    return 1;
  }

  if (assert_true(report.first_u16_le.has_value() && report.first_u16_le.value() == 0x3210,
                  "probe should decode first u16 little-endian") != 0) {
    return 1;
  }

  if (assert_true(report.first_u32_le.has_value() && report.first_u32_le.value() == 0x76543210,
                  "probe should decode first u32 little-endian") != 0) {
    return 1;
  }

  if (assert_true(!report.dos_mz_header.has_value(), "non-MZ buffer should not parse DOS header") != 0) {
    return 1;
  }

  return 0;
}

int test_probe_parses_dos_mz_header_when_present() {
  std::vector<std::uint8_t> bytes(64, 0);
  bytes[0] = 0x4D;  // M
  bytes[1] = 0x5A;  // Z
  bytes[2] = 0x20;  // bytes_in_last_page low
  bytes[3] = 0x01;  // bytes_in_last_page high
  bytes[4] = 0x03;  // pages_in_file low
  bytes[5] = 0x00;  // pages_in_file high
  bytes[24] = 0x40;
  bytes[25] = 0x00;
  bytes[60] = 0x80;
  bytes[61] = 0x00;
  bytes[62] = 0x00;
  bytes[63] = 0x00;

  romulus::data::LoadedFile file;
  file.path = "mz.bin";
  file.bytes = bytes;

  const auto report = romulus::data::probe_loaded_binary(file);
  if (assert_true(report.dos_mz_header.has_value(), "MZ buffer should parse DOS header") != 0) {
    return 1;
  }

  const auto header = report.dos_mz_header.value();
  if (assert_true(header.bytes_in_last_page == 0x0120, "header bytes_in_last_page should match input") != 0) {
    return 1;
  }

  if (assert_true(header.pages_in_file == 3, "header pages_in_file should match input") != 0) {
    return 1;
  }

  if (assert_true(header.relocation_table_offset == 64, "header relocation table offset should match input") != 0) {
    return 1;
  }

  if (assert_true(header.pe_header_offset == 128, "header pe header offset should match input") != 0) {
    return 1;
  }

  const auto rendered = romulus::data::format_binary_probe_report(report);
  return assert_true(rendered.find("dos_mz_header: present") != std::string::npos,
                     "formatted report should include parsed header marker");
}

int test_probe_from_loaded_file() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir);
  const auto file_path = temp_dir / "probe.bin";

  {
    std::ofstream output(file_path, std::ios::binary);
    output.write("AB", 2);
  }

  const auto loaded = romulus::data::load_file_to_memory(file_path);
  if (assert_true(loaded.ok(), "load_file_to_memory should succeed for probe input") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto report = romulus::data::probe_loaded_binary(loaded.value.value());
  if (assert_true(report.first_u16_le.has_value(), "2-byte file should support u16 probe") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(!report.first_u32_le.has_value(), "2-byte file should not support u32 probe") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

}  // namespace

int main() {
  std::srand(4242);

  if (test_probe_reports_basic_scalars() != 0) {
    return EXIT_FAILURE;
  }

  if (test_probe_parses_dos_mz_header_when_present() != 0) {
    return EXIT_FAILURE;
  }

  if (test_probe_from_loaded_file() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
