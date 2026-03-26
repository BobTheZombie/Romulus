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

std::vector<std::uint8_t> make_forum_style_pl8_image_fixture(const std::uint16_t width = 640,
                                                              const std::uint16_t height = 480) {
  const auto payload_size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  std::vector<std::uint8_t> bytes(romulus::data::Pl8ImageResource::kHeaderSize + payload_size, 0);

  bytes[8] = static_cast<std::uint8_t>(width & 0xFFU);
  bytes[9] = static_cast<std::uint8_t>((width >> 8U) & 0xFFU);
  bytes[10] = static_cast<std::uint8_t>(height & 0xFFU);
  bytes[11] = static_cast<std::uint8_t>((height >> 8U) & 0xFFU);

  for (std::size_t i = 0; i < payload_size; ++i) {
    bytes[romulus::data::Pl8ImageResource::kHeaderSize + i] = static_cast<std::uint8_t>(i & 0xFFU);
  }

  return bytes;
}

std::vector<std::uint8_t> make_large_pl8_variant_fixture(const std::uint16_t width,
                                                          const std::uint16_t height,
                                                          const std::size_t payload_size) {
  std::vector<std::uint8_t> bytes(romulus::data::Pl8ImageResource::kHeaderSize + payload_size, 0);
  bytes[8] = static_cast<std::uint8_t>(width & 0xFFU);
  bytes[9] = static_cast<std::uint8_t>((width >> 8U) & 0xFFU);
  bytes[10] = static_cast<std::uint8_t>(height & 0xFFU);
  bytes[11] = static_cast<std::uint8_t>((height >> 8U) & 0xFFU);

  for (std::size_t i = 0; i < payload_size; ++i) {
    bytes[romulus::data::Pl8ImageResource::kHeaderSize + i] = static_cast<std::uint8_t>((i + 17U) & 0xFFU);
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

int test_parse_forum_style_pl8_rejects_zero_dimensions() {
  auto bytes = make_forum_style_pl8_image_fixture();
  bytes[8] = 0;
  bytes[9] = 0;

  const auto parsed = romulus::data::parse_caesar2_forum_pl8_image(bytes);
  if (assert_true(!parsed.ok(), "zero dimensions should fail") != 0) {
    return 1;
  }

  return assert_true(parsed.error->message.find("Unsupported FORUM-style PL8 image layout") != std::string::npos,
                     "invalid dimensions should fail explicitly");
}

int test_parse_forum_style_pl8_accepts_non_640x480_dimensions() {
  const auto bytes = make_forum_style_pl8_image_fixture(320, 200);
  const auto parsed = romulus::data::parse_caesar2_forum_pl8_image(bytes);
  if (assert_true(parsed.ok(), "same family non-640x480 dimensions should parse") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->width == 320 && parsed.value->height == 200,
                  "decoded dimensions should use u16 width/height fields") != 0) {
    return 1;
  }

  return assert_true(parsed.value->payload_size == 64000, "payload should match parsed dimensions");
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

  if (assert_true(report.find("payload_matches_dimensions: yes") != std::string::npos,
                  "report should include explicit payload-match status") != 0) {
    return 1;
  }

  return assert_true(report.find("width: 640") != std::string::npos,
                     "report should include deterministic dimensions");
}

int test_probe_large_pl8_variant_reports_surplus_deterministically() {
  const auto bytes = make_large_pl8_variant_fixture(192, 168, 37568);
  const auto probed = romulus::data::probe_caesar2_large_pl8_image_variant(bytes);
  if (assert_true(probed.ok(), "variant probe should parse bounded shared header") != 0) {
    return 1;
  }

  if (assert_true(probed.value->payload_expected_from_dimensions == 32256,
                  "expected bytes should come from parsed width*height") != 0) {
    return 1;
  }

  if (assert_true(probed.value->payload_surplus_or_deficit == 5312, "surplus should be deterministic") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_pl8_image_variant_probe_report(probed.value.value());
  if (assert_true(report.find("# Caesar II Win95 Large-PL8 Variant Probe Report") != std::string::npos,
                  "report should include stable title") != 0) {
    return 1;
  }

  if (assert_true(report.find("payload_surplus_or_deficit: 5312") != std::string::npos,
                  "report should include deterministic surplus") != 0) {
    return 1;
  }

  return assert_true(report.find("row_stride_hint: unavailable") != std::string::npos,
                     "row-stride hint should be unavailable when payload is not row-divisible");
}

int test_probe_large_pl8_variant_rejects_truncated_header() {
  const std::vector<std::uint8_t> bytes(romulus::data::Pl8ImageResource::kHeaderSize - 1, 0);
  const auto probed = romulus::data::probe_caesar2_large_pl8_image_variant(bytes);
  if (assert_true(!probed.ok(), "truncated header should fail") != 0) {
    return 1;
  }

  return assert_true(probed.error->message.find("expected at least header_size=24") != std::string::npos,
                     "truncated failure should include deterministic bounded error");
}

int test_format_large_pl8_variant_comparison_report_is_stable() {
  const auto forum = make_large_pl8_variant_fixture(192, 168, 32256);
  const auto rat_back = make_large_pl8_variant_fixture(192, 168, 37568);
  const auto forum_probed = romulus::data::probe_caesar2_large_pl8_image_variant(forum);
  const auto rat_probed = romulus::data::probe_caesar2_large_pl8_image_variant(rat_back);
  if (assert_true(forum_probed.ok() && rat_probed.ok(), "both fixtures should probe") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_pl8_image_variant_comparison_report(
      forum_probed.value.value(), "FORUM.PL8", rat_probed.value.value(), "RAT_BACK.PL8");
  if (assert_true(report.find("# Caesar II Win95 Large-PL8 Variant Comparison") != std::string::npos,
                  "comparison report should include stable title") != 0) {
    return 1;
  }
  if (assert_true(report.find("payload_size_delta_rhs_minus_lhs: 5312") != std::string::npos,
                  "comparison should include deterministic payload delta") != 0) {
    return 1;
  }
  return assert_true(report.find("lhs_label: FORUM.PL8") != std::string::npos &&
                         report.find("rhs_label: RAT_BACK.PL8") != std::string::npos,
                     "comparison should include deterministic labels");
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

  if (test_parse_forum_style_pl8_rejects_zero_dimensions() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_forum_style_pl8_accepts_non_640x480_dimensions() != 0) {
    return EXIT_FAILURE;
  }

  if (test_format_pl8_image_report_is_stable() != 0) {
    return EXIT_FAILURE;
  }

  if (test_probe_large_pl8_variant_reports_surplus_deterministically() != 0) {
    return EXIT_FAILURE;
  }

  if (test_probe_large_pl8_variant_rejects_truncated_header() != 0) {
    return EXIT_FAILURE;
  }

  if (test_format_large_pl8_variant_comparison_report_is_stable() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
