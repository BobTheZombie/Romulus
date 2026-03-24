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

std::vector<std::uint8_t> make_valid_map_bytes_fixture() {
  std::vector<std::uint8_t> bytes = {
      'M', 'A', 'P', '0',
      0x01, 0x00,              // format_version = 1
      0x1C, 0x00,              // header_size = 28
      0x04, 0x00,              // width = 4
      0x03, 0x00,              // height = 3
      0x0C, 0x00, 0x00, 0x00,  // terrain_tile_count = 12
      0x04, 0x00, 0x00, 0x00,  // overlay_tile_count = 4
      0x78, 0x56, 0x34, 0x12,  // flags = 0x12345678
      0x44, 0x33, 0x22, 0x11,  // random_seed = 0x11223344
  };

  const std::array<std::uint8_t, 12> terrain = {0, 1, 2, 3, 4, 5, 7, 9, 11, 13, 17, 21};
  bytes.insert(bytes.end(), terrain.begin(), terrain.end());

  return bytes;
}

int test_parse_map_header_success() {
  const auto bytes = make_valid_map_bytes_fixture();
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

  if (assert_true(map.width == 4 && map.height == 3, "dimensions should match fixture") != 0) {
    return 1;
  }

  if (assert_true(map.terrain_tile_count == 12 && map.overlay_tile_count == 4,
                  "tile counts should match fixture") != 0) {
    return 1;
  }

  if (assert_true(map.terrain_tiles.empty(), "header-only parse should not decode terrain payload") != 0) {
    return 1;
  }

  return assert_true(map.flags == 0x12345678 && map.random_seed == 0x11223344,
                     "metadata fields should match fixture");
}

int test_parse_map_decode_terrain_success_and_allows_trailing_bytes() {
  auto bytes = make_valid_map_bytes_fixture();
  bytes.push_back(0xFE);
  bytes.push_back(0xED);

  const auto parsed = romulus::data::parse_caesar2_map(bytes);
  if (assert_true(parsed.ok(), "valid full map should decode") != 0) {
    return 1;
  }

  const auto& map = parsed.value.value();
  if (assert_true(map.terrain_tiles.size() == 12, "decoded terrain tile count should match header") != 0) {
    return 1;
  }

  if (assert_true(map.terrain_tiles[0] == 0 && map.terrain_tiles[5] == 5 && map.terrain_tiles[11] == 21,
                  "decoded terrain values should preserve payload bytes") != 0) {
    return 1;
  }

  return assert_true(map.overlay_tile_count == 4, "full parse should preserve header metadata");
}

int test_parse_map_rejects_truncated_terrain_payload() {
  auto bytes = make_valid_map_bytes_fixture();
  bytes.pop_back();

  const auto parsed = romulus::data::parse_caesar2_map(bytes);
  if (assert_true(!parsed.ok(), "truncated terrain payload should fail") != 0) {
    return 1;
  }

  if (assert_true(parsed.error.has_value() && parsed.error->code == romulus::data::ParseErrorCode::OutOfBounds,
                  "truncated terrain payload should map to out-of-bounds") != 0) {
    return 1;
  }

  return assert_true(parsed.error.has_value() && parsed.error->offset == romulus::data::MapResource::kHeaderSize,
                     "truncated terrain payload should fail at terrain payload start");
}

int test_parse_map_rejects_header_count_mismatch() {
  auto bytes = make_valid_map_bytes_fixture();
  bytes[12] = 0x0B;

  const auto parsed = romulus::data::parse_caesar2_map(bytes);
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

int test_parse_map_header_rejects_bad_magic() {
  auto bytes = make_valid_map_bytes_fixture();
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

int test_parse_map_header_rejects_overlay_count_overflow() {
  auto bytes = make_valid_map_bytes_fixture();
  bytes[16] = 0x0D;

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
  const auto parsed = romulus::data::parse_caesar2_map(make_valid_map_bytes_fixture());
  if (assert_true(parsed.ok(), "report test requires valid parsed map") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_map_report(parsed.value.value());
  if (assert_true(report.find("# Caesar II Map Report") != std::string::npos,
                  "report should include heading") != 0) {
    return 1;
  }

  if (assert_true(report.find("terrain_payload_decoded: 12 bytes") != std::string::npos,
                  "report should include decoded terrain summary") != 0) {
    return 1;
  }

  return assert_true(report.find("terrain_sample[0..7]: 0, 1, 2, 3, 4, 5, 7, 9") != std::string::npos,
                     "report should include terrain sample values");
}

}  // namespace

int main() {
  if (test_parse_map_header_success() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_map_decode_terrain_success_and_allows_trailing_bytes() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_map_rejects_truncated_terrain_payload() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_map_rejects_header_count_mismatch() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_map_header_rejects_bad_magic() != 0) {
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
