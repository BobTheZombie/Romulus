#include "romulus/data/file_loader.h"
#include "romulus/data/palette.h"

#include <cstddef>
#include <cstdint>
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
  return std::filesystem::temp_directory_path() / ("romulus_palette_tests_" + unique);
}

std::vector<std::uint8_t> make_valid_palette_fixture() {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(romulus::data::PaletteResource::kExpectedEntryCount * romulus::data::PaletteResource::kBytesPerEntry);

  for (std::size_t index = 0; index < romulus::data::PaletteResource::kExpectedEntryCount; ++index) {
    bytes.push_back(static_cast<std::uint8_t>(index % 64));
    bytes.push_back(static_cast<std::uint8_t>((index * 2) % 64));
    bytes.push_back(static_cast<std::uint8_t>((index * 3) % 64));
  }

  return bytes;
}

int test_parse_palette_success() {
  const auto bytes = make_valid_palette_fixture();
  const auto parsed = romulus::data::parse_palette_resource(bytes);

  if (assert_true(parsed.ok(), "valid fixture should parse") != 0) {
    return 1;
  }

  const auto& palette = parsed.value.value();
  if (assert_true(palette.entries.size() == romulus::data::PaletteResource::kExpectedEntryCount,
                  "palette should contain 256 entries") != 0) {
    return 1;
  }

  if (assert_true(palette.entries[0].red == 0 && palette.entries[0].green == 0 && palette.entries[0].blue == 0,
                  "entry 0 should match fixture") != 0) {
    return 1;
  }

  if (assert_true(palette.entries[1].red == 1 && palette.entries[1].green == 2 && palette.entries[1].blue == 3,
                  "entry 1 should match fixture") != 0) {
    return 1;
  }

  if (assert_true(palette.entries[63].red == 63 && palette.entries[63].green == 62 && palette.entries[63].blue == 61,
                  "entry 63 should match fixture") != 0) {
    return 1;
  }

  return 0;
}

int test_parse_palette_rejects_non_triplet_byte_count() {
  const std::vector<std::uint8_t> bytes = {1, 2, 3, 4};
  const auto parsed = romulus::data::parse_palette_resource(bytes);

  if (assert_true(!parsed.ok(), "parser should reject non-triplet byte count") != 0) {
    return 1;
  }

  if (assert_true(parsed.error.has_value() && parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                  "parser should return invalid format error for non-triplet byte count") != 0) {
    return 1;
  }

  return 0;
}

int test_parse_palette_rejects_wrong_entry_count() {
  const std::vector<std::uint8_t> bytes(romulus::data::PaletteResource::kBytesPerEntry * 2, 0);
  const auto parsed = romulus::data::parse_palette_resource(bytes);

  if (assert_true(!parsed.ok(), "parser should reject wrong entry count") != 0) {
    return 1;
  }

  if (assert_true(parsed.error.has_value() && parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                  "parser should return invalid format for wrong entry count") != 0) {
    return 1;
  }

  return 0;
}

int test_parse_palette_rejects_component_above_6bit_range() {
  auto bytes = make_valid_palette_fixture();
  bytes[5] = 255;

  const auto parsed = romulus::data::parse_palette_resource(bytes);
  if (assert_true(!parsed.ok(), "parser should reject out-of-range palette components") != 0) {
    return 1;
  }

  if (assert_true(parsed.error.has_value() && parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                  "out-of-range component should map to invalid format") != 0) {
    return 1;
  }

  if (assert_true(parsed.error.has_value() && parsed.error->offset == 3,
                  "error offset should point at first byte of offending palette entry") != 0) {
    return 1;
  }

  return 0;
}

int test_palette_report() {
  const auto bytes = make_valid_palette_fixture();
  const auto parsed = romulus::data::parse_palette_resource(bytes);
  if (assert_true(parsed.ok(), "report test requires valid palette") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_palette_report(parsed.value.value(), 2);
  if (assert_true(report.find("# Caesar II Palette Report") != std::string::npos,
                  "report should include heading") != 0) {
    return 1;
  }

  if (assert_true(report.find("entry_count: 256") != std::string::npos,
                  "report should include entry count") != 0) {
    return 1;
  }

  if (assert_true(report.find("... (254 more entries)") != std::string::npos,
                  "report should include truncation marker") != 0) {
    return 1;
  }

  return 0;
}

int test_parse_palette_from_loaded_file() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir);
  const auto file_path = temp_dir / "palette.bin";

  const auto bytes = make_valid_palette_fixture();
  {
    std::ofstream output(file_path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }

  const auto loaded = romulus::data::load_file_to_memory(file_path);
  if (assert_true(loaded.ok(), "load_file_to_memory should succeed") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto parsed = romulus::data::parse_palette_resource(loaded.value->bytes);
  if (assert_true(parsed.ok(), "loaded file bytes should parse as palette") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

}  // namespace

int main() {
  std::srand(10101);

  if (test_parse_palette_success() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_palette_rejects_non_triplet_byte_count() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_palette_rejects_wrong_entry_count() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_palette_rejects_component_above_6bit_range() != 0) {
    return EXIT_FAILURE;
  }

  if (test_palette_report() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_palette_from_loaded_file() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
