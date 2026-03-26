#include "romulus/data/pl8_image_resource.h"

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

std::vector<std::uint8_t> make_forum_style_pl8_image_fixture() {
  std::vector<std::uint8_t> bytes(romulus::data::Pl8ImageResource::kSupportedFileSize, 0);

  const std::uint32_t width = romulus::data::Pl8ImageResource::kSupportedWidth;
  const std::uint32_t height = romulus::data::Pl8ImageResource::kSupportedHeight;
  bytes[8] = static_cast<std::uint8_t>(width & 0xFFU);
  bytes[9] = static_cast<std::uint8_t>((width >> 8U) & 0xFFU);
  bytes[10] = static_cast<std::uint8_t>((width >> 16U) & 0xFFU);
  bytes[11] = static_cast<std::uint8_t>((width >> 24U) & 0xFFU);

  bytes[12] = static_cast<std::uint8_t>(height & 0xFFU);
  bytes[13] = static_cast<std::uint8_t>((height >> 8U) & 0xFFU);
  bytes[14] = static_cast<std::uint8_t>((height >> 16U) & 0xFFU);
  bytes[15] = static_cast<std::uint8_t>((height >> 24U) & 0xFFU);

  for (std::size_t i = 0; i < romulus::data::Pl8ImageResource::kSupportedPayloadSize; ++i) {
    bytes[romulus::data::Pl8ImageResource::kHeaderSize + i] = static_cast<std::uint8_t>(i & 0xFFU);
  }

  return bytes;
}

std::vector<std::uint8_t> make_256_palette_fixture() {
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

int test_parse_forum_style_pl8_image_success() {
  const auto bytes = make_forum_style_pl8_image_fixture();
  const auto parsed = romulus::data::parse_caesar2_forum_pl8_image(bytes);

  if (assert_true(parsed.ok(), "valid FORUM-style PL8 image should parse") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->width == 640 && parsed.value->height == 480,
                  "decoded dimensions should be 640x480") != 0) {
    return 1;
  }

  return assert_true(parsed.value->payload_size == 307200, "payload should be 640*480 bytes");
}

int test_parse_forum_style_pl8_rejects_small_header() {
  const std::vector<std::uint8_t> bytes(romulus::data::Pl8ImageResource::kHeaderSize - 1, 0);
  const auto parsed = romulus::data::parse_caesar2_forum_pl8_image(bytes);

  if (assert_true(!parsed.ok(), "sub-header payload should fail") != 0) {
    return 1;
  }

  return assert_true(parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                     "sub-header failure should be invalid format");
}

int test_parse_forum_style_pl8_rejects_payload_mismatch() {
  auto bytes = make_forum_style_pl8_image_fixture();
  bytes.pop_back();

  const auto parsed = romulus::data::parse_caesar2_forum_pl8_image(bytes);
  if (assert_true(!parsed.ok(), "payload mismatch should fail") != 0) {
    return 1;
  }

  return assert_true(parsed.error->message.find("payload mismatch") != std::string::npos,
                     "payload mismatch should report deterministic reason");
}

int test_parse_forum_style_pl8_rejects_unsupported_dimensions() {
  auto bytes = make_forum_style_pl8_image_fixture();
  bytes[8] = 0x20;
  bytes[9] = 0x03;
  bytes[12] = 0x58;
  bytes[13] = 0x02;

  const auto parsed = romulus::data::parse_caesar2_forum_pl8_image(bytes);
  if (assert_true(!parsed.ok(), "unsupported dimensions should fail") != 0) {
    return 1;
  }

  return assert_true(parsed.error->message.find("Unsupported FORUM-style PL8 image layout") != std::string::npos,
                     "unsupported dimensions should fail explicitly");
}

int test_decode_forum_style_pl8_with_256_palette_success() {
  const auto image = make_forum_style_pl8_image_fixture();
  const auto palette = make_256_palette_fixture();

  const auto decoded = romulus::data::decode_caesar2_forum_pl8_image_pair(image, palette, true);
  if (assert_true(decoded.ok(), "FORUM-style PL8 + .256 palette should decode") != 0) {
    return 1;
  }

  if (assert_true(decoded.value->rgba_image.width == 640 && decoded.value->rgba_image.height == 480,
                  "decoded RGBA image should preserve 640x480") != 0) {
    return 1;
  }

  return assert_true(decoded.value->rgba_image.pixels_rgba[3] == 0,
                     "index zero should become transparent when requested");
}

int test_format_pl8_image_report_is_stable() {
  const auto bytes = make_forum_style_pl8_image_fixture();
  const auto parsed = romulus::data::parse_caesar2_forum_pl8_image(bytes);
  if (assert_true(parsed.ok(), "fixture should parse before report formatting") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_pl8_image_report(parsed.value.value(), 4);
  if (assert_true(report.find("# Caesar II Win95 FORUM-style PL8 Image Report") != std::string::npos,
                  "report should include stable title") != 0) {
    return 1;
  }

  if (assert_true(report.find("supported_layout: forum_pl8_image_24b_header") != std::string::npos,
                  "report should include stable layout label") != 0) {
    return 1;
  }

  return assert_true(report.find("width: 640") != std::string::npos,
                     "report should include deterministic dimensions");
}

}  // namespace

int main() {
  if (test_parse_forum_style_pl8_image_success() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_forum_style_pl8_rejects_small_header() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_forum_style_pl8_rejects_payload_mismatch() != 0) {
    return EXIT_FAILURE;
  }

  if (test_decode_forum_style_pl8_with_256_palette_success() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_forum_style_pl8_rejects_unsupported_dimensions() != 0) {
    return EXIT_FAILURE;
  }

  if (test_format_pl8_image_report_is_stable() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
