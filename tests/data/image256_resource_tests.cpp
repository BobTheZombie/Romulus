#include "romulus/data/image256_resource.h"

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

std::vector<std::uint8_t> make_supported_pl8_fixture() {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(romulus::data::Pl8Resource::kSupportedPayloadSize);

  for (std::size_t index = 0; index < romulus::data::Pl8Resource::kSupportedEntryCount; ++index) {
    const auto component = static_cast<std::uint8_t>(index % 64);
    bytes.push_back(component);
    bytes.push_back(component);
    bytes.push_back(component);
  }

  return bytes;
}

int test_parse_256_success_with_explicit_dimensions() {
  const std::vector<std::uint8_t> bytes = {0, 1, 2, 3, 4, 5};
  const auto parsed = romulus::data::parse_caesar2_win95_raw_256(bytes, 3, 2);

  if (assert_true(parsed.ok(), "valid .256 payload should parse with explicit dimensions") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->indexed_pixels == bytes, "indexed pixels should copy exactly") != 0) {
    return 1;
  }

  return assert_true(parsed.value->payload_size == 6, "payload size should be preserved");
}

int test_parse_256_rejects_payload_size_mismatch() {
  const std::vector<std::uint8_t> bytes = {0, 1, 2};
  const auto parsed = romulus::data::parse_caesar2_win95_raw_256(bytes, 2, 2);

  if (assert_true(!parsed.ok(), "mismatched payload size should fail") != 0) {
    return 1;
  }

  return assert_true(parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                     "size mismatch should be invalid format");
}

int test_decode_256_pl8_pair_success() {
  const std::vector<std::uint8_t> image = {0, 1, 2, 3};
  const auto pl8 = make_supported_pl8_fixture();

  const auto decoded = romulus::data::decode_caesar2_win95_256_pl8_pair(image, pl8, 2, 2, true);
  if (assert_true(decoded.ok(), "valid .256 + .PL8 pair should decode") != 0) {
    return 1;
  }

  if (assert_true(decoded.value->rgba_image.pixels_rgba.size() == 16, "rgba output should have 4 bytes per pixel") != 0) {
    return 1;
  }

  return assert_true(decoded.value->rgba_image.pixels_rgba[3] == 0,
                     "index zero should become transparent when requested");
}

int test_decode_256_pl8_pair_rejects_malformed_palette() {
  const std::vector<std::uint8_t> image = {0, 1, 2, 3};
  const std::vector<std::uint8_t> malformed_palette = {1, 2, 3, 4, 5};

  const auto decoded = romulus::data::decode_caesar2_win95_256_pl8_pair(image, malformed_palette, 2, 2);
  if (assert_true(!decoded.ok(), "malformed palette should fail") != 0) {
    return 1;
  }

  return assert_true(decoded.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                     "malformed palette should surface parse failure");
}

int test_format_pair_report_is_stable() {
  romulus::data::Image256Pl8Report report{
      .image256_path = "DATA0/FORUM.256",
      .palette_pl8_path = "DATA0/FORUM.PL8",
      .width = 640,
      .height = 480,
      .payload_size = 307200,
      .palette_entries = 256,
      .success = true,
      .status = "decode_succeeded",
  };

  const auto formatted = romulus::data::format_image256_pl8_report(report);
  if (assert_true(formatted.find("# Caesar II Win95 .256+.PL8 Decode Report") != std::string::npos,
                  "report header should be stable") != 0) {
    return 1;
  }

  if (assert_true(formatted.find("image_path: DATA0/FORUM.256") != std::string::npos,
                  "report should include image path") != 0) {
    return 1;
  }

  return assert_true(formatted.find("status: ok") != std::string::npos,
                     "report should include deterministic status line");
}

int test_known_dimension_helper() {
  const auto forum = romulus::data::resolve_known_win95_256_dimensions("DATA0/FORUM.256");
  if (assert_true(forum.has_value(), "known helper should include FORUM.256") != 0) {
    return 1;
  }

  if (assert_true(forum->first == 640 && forum->second == 480, "FORUM.256 should map to 640x480") != 0) {
    return 1;
  }

  const auto unknown = romulus::data::resolve_known_win95_256_dimensions("DATA0/UNKNOWN.256");
  return assert_true(!unknown.has_value(), "unknown files should not guess dimensions");
}

}  // namespace

int main() {
  if (test_parse_256_success_with_explicit_dimensions() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_256_rejects_payload_size_mismatch() != 0) {
    return EXIT_FAILURE;
  }

  if (test_decode_256_pl8_pair_success() != 0) {
    return EXIT_FAILURE;
  }

  if (test_decode_256_pl8_pair_rejects_malformed_palette() != 0) {
    return EXIT_FAILURE;
  }

  if (test_format_pair_report_is_stable() != 0) {
    return EXIT_FAILURE;
  }

  if (test_known_dimension_helper() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
