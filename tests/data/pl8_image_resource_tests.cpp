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

std::vector<std::uint8_t> make_structured_pl8_fixture(const std::uint16_t width = 96,
                                                       const std::uint16_t height = 337,
                                                       const std::uint32_t image_offset = 16,
                                                       const std::uint32_t image_size = 0,
                                                       const std::size_t trailing_bytes = 32,
                                                       const bool malformed_prefix = false) {
  const auto expected_image_size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  const auto effective_image_size = image_size == 0 ? expected_image_size : static_cast<std::size_t>(image_size);
  const auto payload_size = malformed_prefix ? 8 : (image_offset + effective_image_size + trailing_bytes);

  std::vector<std::uint8_t> bytes(romulus::data::Pl8ImageResource::kHeaderSize + payload_size, 0);
  bytes[8] = static_cast<std::uint8_t>(width & 0xFFU);
  bytes[9] = static_cast<std::uint8_t>((width >> 8U) & 0xFFU);
  bytes[10] = static_cast<std::uint8_t>(height & 0xFFU);
  bytes[11] = static_cast<std::uint8_t>((height >> 8U) & 0xFFU);

  if (!malformed_prefix) {
    const std::uint32_t field_values[4] = {
        image_offset,
        static_cast<std::uint32_t>(effective_image_size),
        0x01020304U,
        0xA5A5A5A5U,
    };

    for (std::size_t field_index = 0; field_index < 4; ++field_index) {
      const auto field_offset = romulus::data::Pl8ImageResource::kHeaderSize + field_index * 4;
      bytes[field_offset + 0] = static_cast<std::uint8_t>(field_values[field_index] & 0xFFU);
      bytes[field_offset + 1] = static_cast<std::uint8_t>((field_values[field_index] >> 8U) & 0xFFU);
      bytes[field_offset + 2] = static_cast<std::uint8_t>((field_values[field_index] >> 16U) & 0xFFU);
      bytes[field_offset + 3] = static_cast<std::uint8_t>((field_values[field_index] >> 24U) & 0xFFU);
    }

    if (image_offset + effective_image_size <= payload_size) {
      for (std::size_t i = 0; i < effective_image_size; ++i) {
        bytes[romulus::data::Pl8ImageResource::kHeaderSize + image_offset + i] = static_cast<std::uint8_t>(i & 0xFFU);
      }
    }
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

int test_parse_structured_pl8_success() {
  const auto bytes = make_structured_pl8_fixture();
  const auto parsed = romulus::data::parse_caesar2_rat_back_structured_pl8_image(bytes);
  if (assert_true(parsed.ok(), "structured fixture should parse") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->selected_region.has_value(), "structured fixture should isolate one image region") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->selected_region->payload_offset == 16, "selected region offset should be deterministic") != 0) {
    return 1;
  }

  return assert_true(parsed.value->indexed_pixels.size() == parsed.value->expected_image_size,
                     "decoded indexed pixels should match expected area");
}

int test_parse_structured_pl8_rejects_malformed_prefix() {
  const auto bytes = make_structured_pl8_fixture(96, 337, 16, 0, 32, true);
  const auto parsed = romulus::data::parse_caesar2_rat_back_structured_pl8_image(bytes);
  if (assert_true(!parsed.ok(), "malformed prefix fixture should fail") != 0) {
    return 1;
  }

  return assert_true(parsed.error->message.find("structured payload too small") != std::string::npos,
                     "malformed prefix failure should be explicit");
}

int test_parse_structured_pl8_rejects_invalid_candidate_offset() {
  const auto bytes = make_structured_pl8_fixture(96, 337, 4);
  const auto parsed = romulus::data::parse_caesar2_rat_back_structured_pl8_image(bytes);
  if (assert_true(!parsed.ok(), "candidate that points into prefix should fail") != 0) {
    return 1;
  }

  return assert_true(parsed.error->message.find("did not yield a deterministic image region") != std::string::npos,
                     "invalid candidate should fail with deterministic reason");
}

int test_format_structured_pl8_report_is_stable() {
  const auto bytes = make_structured_pl8_fixture();
  const auto parsed = romulus::data::parse_caesar2_rat_back_structured_pl8_image(bytes);
  if (assert_true(parsed.ok(), "structured fixture should parse before report formatting") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_pl8_structured_report(parsed.value.value());
  if (assert_true(report.find("# Caesar II Win95 RAT_BACK-style Structured PL8 Report") != std::string::npos,
                  "structured report should include stable title") != 0) {
    return 1;
  }

  if (assert_true(report.find("field_0_u32le: 16") != std::string::npos,
                  "structured report should include deterministic field output") != 0) {
    return 1;
  }

  return assert_true(report.find("primary_image_region_identified: yes") != std::string::npos,
                     "structured report should include decode status");
}

int test_decode_structured_pl8_with_256_palette_success() {
  const auto image = make_structured_pl8_fixture();
  const auto palette = make_256_palette_fixture();
  const auto decoded = romulus::data::decode_caesar2_rat_back_structured_pl8_image_pair(image, palette, true);
  if (assert_true(decoded.ok(), "structured PL8 + .256 should decode") != 0) {
    return 1;
  }

  if (assert_true(decoded.value->rgba_image.width == 96 && decoded.value->rgba_image.height == 337,
                  "decoded structured image should preserve dimensions") != 0) {
    return 1;
  }

  return assert_true(decoded.value->rgba_image.pixels_rgba[3] == 0,
                     "structured decode should respect index-zero transparency");
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

  if (test_parse_structured_pl8_success() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_structured_pl8_rejects_malformed_prefix() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_structured_pl8_rejects_invalid_candidate_offset() != 0) {
    return EXIT_FAILURE;
  }

  if (test_format_structured_pl8_report_is_stable() != 0) {
    return EXIT_FAILURE;
  }

  if (test_decode_structured_pl8_with_256_palette_success() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
