#include "romulus/data/map_resource.h"

#include <array>
#include <cstdint>
#include <cstdlib>
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

std::vector<std::uint8_t> make_valid_map_header_fixture() {
  return {
      'M', 'A', 'P', '0',
      0x01, 0x00,              // format_version = 1
      0x1C, 0x00,              // header_size = 28
      0x50, 0x00,              // width = 80
      0x40, 0x00,              // height = 64
      0x00, 0x14, 0x00, 0x00,  // terrain_tile_count = 5120
      0x00, 0x04, 0x00, 0x00,  // overlay_tile_count = 1024
      0x78, 0x56, 0x34, 0x12,  // flags = 0x12345678
      0x44, 0x33, 0x22, 0x11,  // random_seed = 0x11223344
  };
}

int test_parse_map_header_success() {
  const auto bytes = make_valid_map_header_fixture();
  const auto parsed = romulus::data::parse_caesar2_map_header(bytes);

  if (assert_true(parsed.ok(), "valid map header fixture should parse") != 0) {
    return 1;
  }

  const auto& map = parsed.value.value();
  if (assert_true(map.format_version == 1, "format_version should match fixture") != 0) {
    return 1;
  }

  if (assert_true(map.header_size == romulus::data::MapResource::kHeaderSize,
                  "header_size should match known format") != 0) {
    return 1;
  }

  if (assert_true(map.width == 80 && map.height == 64, "dimensions should match fixture") != 0) {
    return 1;
  }

  if (assert_true(map.terrain_tile_count == 5120 && map.overlay_tile_count == 1024,
                  "tile counts should match fixture") != 0) {
    return 1;
  }

  return assert_true(map.flags == 0x12345678 && map.random_seed == 0x11223344,
                     "metadata fields should match fixture");
}

int test_parse_map_header_rejects_bad_magic() {
  auto bytes = make_valid_map_header_fixture();
  bytes[0] = 'X';

  const auto parsed = romulus::data::parse_caesar2_map_header(bytes);
  if (assert_true(!parsed.ok(), "bad magic should fail parse") != 0) {
    return 1;
  }

  if (assert_true(parsed.error.has_value() && parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                  "bad magic should map to invalid format") != 0) {
    return 1;
  }

  return assert_true(parsed.error.has_value() && parsed.error->offset == 0,
                     "bad magic should fail at map magic offset");
}

int test_parse_map_header_rejects_malformed_counts() {
  auto bytes = make_valid_map_header_fixture();
  bytes[12] = 0xFF;

  const auto parsed = romulus::data::parse_caesar2_map_header(bytes);
  if (assert_true(!parsed.ok(), "terrain count mismatch should fail") != 0) {
    return 1;
  }

  if (assert_true(parsed.error.has_value() && parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                  "terrain count mismatch should map to invalid format") != 0) {
    return 1;
  }

  return assert_true(parsed.error.has_value() && parsed.error->offset == 12,
                     "terrain count mismatch should report terrain count offset");
}

int test_parse_map_header_rejects_overlay_count_overflow() {
  auto bytes = make_valid_map_header_fixture();
  bytes[16] = 0x01;
  bytes[17] = 0x14;

  const auto parsed = romulus::data::parse_caesar2_map_header(bytes);
  if (assert_true(!parsed.ok(), "overlay tile count > terrain count should fail") != 0) {
    return 1;
  }

  return assert_true(parsed.error.has_value() && parsed.error->offset == 16,
                     "overlay tile count overflow should report overlay offset");
}

int test_parse_map_header_rejects_truncated_input() {
  const std::array<std::uint8_t, 6> bytes = {'M', 'A', 'P', '0', 0x01, 0x00};
  const auto parsed = romulus::data::parse_caesar2_map_header(bytes);

  if (assert_true(!parsed.ok(), "truncated input should fail") != 0) {
    return 1;
  }

  if (assert_true(parsed.error.has_value() && parsed.error->code == romulus::data::ParseErrorCode::OutOfBounds,
                  "truncated input should map to out-of-bounds") != 0) {
    return 1;
  }

  return assert_true(parsed.error.has_value() && parsed.error->offset == 6,
                     "truncated parse error should report failing read offset");
}

int test_map_report() {
  const auto parsed = romulus::data::parse_caesar2_map_header(make_valid_map_header_fixture());
  if (assert_true(parsed.ok(), "report test requires valid parsed map") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_map_report(parsed.value.value());
  if (assert_true(report.find("# Caesar II Map Header Report") != std::string::npos,
                  "report should include heading") != 0) {
    return 1;
  }

  if (assert_true(report.find("terrain_tile_count: 5120") != std::string::npos,
                  "report should include terrain tile count") != 0) {
    return 1;
  }

  return assert_true(report.find("flags: 0x12345678") != std::string::npos,
                     "report should include metadata fields");
}

}  // namespace

int main() {
  if (test_parse_map_header_success() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_map_header_rejects_bad_magic() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_map_header_rejects_malformed_counts() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_map_header_rejects_overlay_count_overflow() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_map_header_rejects_truncated_input() != 0) {
    return EXIT_FAILURE;
  }

  if (test_map_report() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
