#include "romulus/data/pl8_sprite_table_resource.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int assert_true(const bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    return 1;
  }
  return 0;
}

void write_u16_le(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint16_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
  bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void write_u32_le(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
  bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
  bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
  bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

std::vector<std::uint8_t> make_pl8_sprite_fixture(const std::uint16_t tile_type = 0,
                                                  const std::uint16_t flags = 0,
                                                  const std::uint32_t data_offset = 24,
                                                  const bool truncate_data = false) {
  std::vector<std::uint8_t> bytes(28, 0);
  write_u16_le(bytes, 0, flags);
  write_u16_le(bytes, 2, 1);
  write_u32_le(bytes, 4, 0x01020304U);

  write_u16_le(bytes, 8, 2);
  write_u16_le(bytes, 10, 2);
  write_u32_le(bytes, 12, data_offset);
  write_u16_le(bytes, 16, 3);
  write_u16_le(bytes, 18, 4);
  write_u16_le(bytes, 20, tile_type);
  write_u16_le(bytes, 22, 7);

  if (!truncate_data) {
    bytes[24] = 0;
    bytes[25] = 1;
    bytes[26] = 2;
    bytes[27] = 3;
  }

  return bytes;
}

std::vector<std::uint8_t> make_multi_sprite_fixture() {
  std::vector<std::uint8_t> bytes(8 + (3 * 16) + 12, 0);
  write_u16_le(bytes, 0, 0);
  write_u16_le(bytes, 2, 3);
  write_u32_le(bytes, 4, 0);

  const std::size_t data_start = 8 + (3 * 16);

  // sprite[0] at (1, 0), 2x2
  write_u16_le(bytes, 8, 2);
  write_u16_le(bytes, 10, 2);
  write_u32_le(bytes, 12, static_cast<std::uint32_t>(data_start));
  write_u16_le(bytes, 16, 1);
  write_u16_le(bytes, 18, 0);
  write_u16_le(bytes, 20, 0);
  write_u16_le(bytes, 22, 0);

  // sprite[1] at (0, 1), 2x2 - overlaps sprite[0] at (1,1)
  write_u16_le(bytes, 24, 2);
  write_u16_le(bytes, 26, 2);
  write_u32_le(bytes, 28, static_cast<std::uint32_t>(data_start + 4));
  write_u16_le(bytes, 32, 0);
  write_u16_le(bytes, 34, 1);
  write_u16_le(bytes, 36, 0);
  write_u16_le(bytes, 38, 0);

  // sprite[2] at (2, 1), 2x2
  write_u16_le(bytes, 40, 2);
  write_u16_le(bytes, 42, 2);
  write_u32_le(bytes, 44, static_cast<std::uint32_t>(data_start + 8));
  write_u16_le(bytes, 48, 2);
  write_u16_le(bytes, 50, 1);
  write_u16_le(bytes, 52, 0);
  write_u16_le(bytes, 54, 0);

  bytes[data_start] = 1;
  bytes[data_start + 1] = 2;
  bytes[data_start + 2] = 3;
  bytes[data_start + 3] = 4;
  bytes[data_start + 4] = 0;
  bytes[data_start + 5] = 5;
  bytes[data_start + 6] = 6;
  bytes[data_start + 7] = 7;
  bytes[data_start + 8] = 8;
  bytes[data_start + 9] = 9;
  bytes[data_start + 10] = 10;
  bytes[data_start + 11] = 11;

  return bytes;
}

std::vector<std::uint8_t> make_256_palette_fixture() {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(romulus::data::Pl8Resource::kSupportedPayloadSize);
  for (std::size_t i = 0; i < romulus::data::Pl8Resource::kSupportedEntryCount; ++i) {
    const auto component = static_cast<std::uint8_t>(i % 64);
    bytes.push_back(component);
    bytes.push_back(component);
    bytes.push_back(component);
  }
  return bytes;
}

int test_parse_sprite_header_and_descriptor_success() {
  const auto bytes = make_pl8_sprite_fixture();
  const auto parsed = romulus::data::parse_caesar2_pl8_sprite_table(bytes);
  if (assert_true(parsed.ok(), "valid PL8 sprite-table should parse") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->header.flags == 0 && parsed.value->header.sprite_count == 1,
                  "header fields should parse deterministically") != 0) {
    return 1;
  }

  const auto& sprite = parsed.value->sprites.front();
  return assert_true(sprite.width == 2 && sprite.height == 2 && sprite.data_offset == 24 && sprite.tile_type == 0,
                     "descriptor fields should parse deterministically");
}

int test_decode_type0_sprite_success() {
  const auto bytes = make_pl8_sprite_fixture();
  const auto decoded = romulus::data::decode_caesar2_pl8_type0_sprite(bytes, 0);
  if (assert_true(decoded.ok(), "type-0 sprite decode should succeed") != 0) {
    return 1;
  }

  return assert_true(decoded.value->indexed_pixels.size() == 4 && decoded.value->indexed_pixels[3] == 3,
                     "type-0 decode should read width*height bytes from data offset");
}

int test_parse_fails_for_truncated_descriptor_table() {
  auto bytes = make_pl8_sprite_fixture();
  bytes.resize(10);

  const auto parsed = romulus::data::parse_caesar2_pl8_sprite_table(bytes);
  if (assert_true(!parsed.ok(), "truncated descriptor table should fail") != 0) {
    return 1;
  }

  return assert_true(parsed.error->message.find("descriptor block truncated") != std::string::npos,
                     "truncated descriptor failure should be explicit");
}

int test_decode_fails_for_invalid_offset_or_size() {
  const auto bytes = make_pl8_sprite_fixture(0, 0, 26, true);
  const auto decoded = romulus::data::decode_caesar2_pl8_type0_sprite(bytes, 0);

  if (assert_true(!decoded.ok(), "invalid data offset/size should fail") != 0) {
    return 1;
  }

  return assert_true(decoded.error->message.find("out of bounds") != std::string::npos,
                     "invalid data offset/size should report deterministic reason");
}

int test_decode_fails_for_unsupported_tile_type() {
  const auto bytes = make_pl8_sprite_fixture(2);
  const auto decoded = romulus::data::decode_caesar2_pl8_type0_sprite(bytes, 0);
  if (assert_true(!decoded.ok(), "unsupported tile type should fail") != 0) {
    return 1;
  }

  return assert_true(decoded.error->message.find("only type 0 is supported") != std::string::npos,
                     "unsupported tile type should report deterministic reason");
}

int test_decode_fails_for_rle_flag() {
  const auto bytes = make_pl8_sprite_fixture(0, 1);
  const auto decoded = romulus::data::decode_caesar2_pl8_type0_sprite(bytes, 0);
  if (assert_true(!decoded.ok(), "RLE-flagged sprite decode should fail explicitly") != 0) {
    return 1;
  }

  return assert_true(decoded.error->message.find("indicates RLE encoding") != std::string::npos,
                     "RLE decode failure should be explicit");
}

int test_report_format_is_stable() {
  const auto bytes = make_pl8_sprite_fixture(2);
  const auto parsed = romulus::data::parse_caesar2_pl8_sprite_table(bytes);
  if (assert_true(parsed.ok(), "fixture should parse before reporting") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_pl8_sprite_table_report(parsed.value.value());
  if (assert_true(report.find("# Caesar II Win95 PL8 Sprite-Table Report") != std::string::npos,
                  "report should contain stable title") != 0) {
    return 1;
  }

  return assert_true(report.find("decode=unsupported_tile_type") != std::string::npos,
                     "report should include deterministic decode status");
}

int test_sprite_pair_decode_with_256_palette_success() {
  const auto image = make_pl8_sprite_fixture();
  const auto palette = make_256_palette_fixture();
  const auto decoded = romulus::data::decode_caesar2_pl8_sprite_pair(image, palette, 0, true);
  if (assert_true(decoded.ok(), "sprite-table + .256 pair should decode") != 0) {
    return 1;
  }

  if (assert_true(decoded.value->rgba_image.width == 2 && decoded.value->rgba_image.height == 2,
                  "RGBA decode should preserve sprite dimensions") != 0) {
    return 1;
  }

  return assert_true(decoded.value->rgba_image.pixels_rgba[3] == 0,
                     "index 0 should be transparent when requested");
}

int test_multi_sprite_decode_and_compose_success() {
  const auto image = make_multi_sprite_fixture();
  const auto palette = make_256_palette_fixture();
  const auto decoded = romulus::data::decode_caesar2_pl8_sprite_pair_multi(image, palette, true);
  if (assert_true(decoded.ok(), "multi-sprite decode should succeed") != 0) {
    return 1;
  }

  if (assert_true(decoded.value->decoded_sprites.size() == 3, "all type-0 sprites should decode") != 0) {
    return 1;
  }

  if (assert_true(decoded.value->composition.has_value(), "successful decode should produce composition") != 0) {
    return 1;
  }

  const auto& composition = decoded.value->composition.value();
  if (assert_true(composition.rgba_image.width == 4 && composition.rgba_image.height == 3,
                  "composition canvas should use descriptor bounds") != 0) {
    return 1;
  }

  return assert_true(composition.bounds.min_x == 0 && composition.bounds.min_y == 0 &&
                         composition.bounds.max_x == 4 && composition.bounds.max_y == 3,
                     "composed bounds should be deterministic");
}

int test_multi_sprite_composition_order_is_deterministic() {
  const auto image = make_multi_sprite_fixture();
  const auto palette = make_256_palette_fixture();
  const auto decoded = romulus::data::decode_caesar2_pl8_sprite_pair_multi(image, palette, true);
  if (assert_true(decoded.ok() && decoded.value->composition.has_value(), "composition should succeed") != 0) {
    return 1;
  }

  // pixel (1,1) is covered by sprite[0] index=4 then sprite[1] index=5, so descriptor-order overwrite => index 5.
  const auto& rgba = decoded.value->composition->rgba_image;
  const std::size_t offset = ((1 * rgba.width) + 1) * 4;
  return assert_true(rgba.pixels_rgba[offset] == 20, "later sprite should deterministically overwrite overlap");
}

int test_multi_sprite_index_zero_transparency() {
  const auto image = make_multi_sprite_fixture();
  const auto palette = make_256_palette_fixture();
  const auto decoded = romulus::data::decode_caesar2_pl8_sprite_pair_multi(image, palette, true);
  if (assert_true(decoded.ok() && decoded.value->composition.has_value(), "composition should succeed") != 0) {
    return 1;
  }

  // sprite[1] pixel (0,0) is index 0 at composed location (0,1), should remain transparent.
  const auto& rgba = decoded.value->composition->rgba_image;
  const std::size_t offset = ((1 * rgba.width) + 0) * 4 + 3;
  return assert_true(rgba.pixels_rgba[offset] == 0, "index 0 should be transparent in composition");
}

int test_multi_sprite_invalid_offset_fails() {
  auto image = make_multi_sprite_fixture();
  write_u32_le(image, 44, static_cast<std::uint32_t>(image.size() - 1));
  const auto palette = make_256_palette_fixture();
  const auto decoded = romulus::data::decode_caesar2_pl8_sprite_pair_multi(image, palette, true);
  if (assert_true(decoded.ok(), "mixed support should still return deterministic report") != 0) {
    return 1;
  }

  if (assert_true(!decoded.value->decode_supported, "invalid sprite should mark decode unsupported") != 0) {
    return 1;
  }

  return assert_true(decoded.value->sprite_reports[2].composition_status == "decode_failed",
                     "invalid sprite should report decode_failed status");
}

int test_multi_sprite_report_format_is_stable() {
  const auto image = make_multi_sprite_fixture();
  const auto palette = make_256_palette_fixture();
  const auto decoded = romulus::data::decode_caesar2_pl8_sprite_pair_multi(image, palette, true);
  if (assert_true(decoded.ok(), "multi decode should succeed for report") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_pl8_sprite_pair_multi_report(decoded.value.value());
  if (assert_true(report.find("sprite_decode_supported: yes") != std::string::npos,
                  "report should include decode support field") != 0) {
    return 1;
  }
  if (assert_true(report.find("image.header.sprite_count: 3") != std::string::npos,
                  "report should include image header fields") != 0) {
    return 1;
  }
  if (assert_true(report.find("composed_bounds: min=(0,0) max=(4,3)") != std::string::npos,
                  "report should include composed bounds") != 0) {
    return 1;
  }
  return assert_true(report.find(
                         "sprite[1]: width=2, height=2, x=0, y=1, tile_type=0, extra_rows=0, decode=supported_type0, "
                         "compose=composed") != std::string::npos,
                     "report should include per-sprite descriptor + decode/composition status");
}

int test_single_sprite_probe_report_is_stable() {
  const auto image = make_multi_sprite_fixture();
  const auto parsed = romulus::data::parse_caesar2_pl8_sprite_table(image);
  if (assert_true(parsed.ok(), "single-sprite report fixture should parse") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_pl8_sprite_table_report_for_sprite(parsed.value.value(), 2);
  if (assert_true(report.find("sprite[2]: width=2, height=2, data_offset=64, x=2, y=1, tile_type=0, extra_rows=0, "
                              "decode=supported_type0") != std::string::npos,
                  "single-sprite report should include only requested sprite row") != 0) {
    return 1;
  }

  return assert_true(report.find("sprite[0]:") == std::string::npos,
                     "single-sprite report should omit non-requested sprite rows");
}

int test_multi_sprite_report_shows_clean_failure_reason() {
  auto image = make_multi_sprite_fixture();
  write_u16_le(image, 36, 3);
  const auto palette = make_256_palette_fixture();
  const auto decoded = romulus::data::decode_caesar2_pl8_sprite_pair_multi(image, palette, true);
  if (assert_true(decoded.ok(), "mixed-support report should still be produced") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_pl8_sprite_pair_multi_report(decoded.value.value());
  return assert_true(report.find("sprite[1]: width=2, height=2, x=0, y=1, tile_type=3, extra_rows=0, "
                                 "decode=unsupported_tile_type, compose=decode_failed") != std::string::npos,
                     "failed sprite should report explicit deterministic reason");
}

int test_sprite_pair_decode_selected_index_from_multi_sprite_fixture() {
  const auto image = make_multi_sprite_fixture();
  const auto palette = make_256_palette_fixture();
  const auto decoded = romulus::data::decode_caesar2_pl8_sprite_pair(image, palette, 1, true);
  if (assert_true(decoded.ok(), "single-sprite decode by explicit index should succeed") != 0) {
    return 1;
  }

  if (assert_true(decoded.value->sprite_index == 1, "decode result should preserve requested sprite index") != 0) {
    return 1;
  }

  return assert_true(decoded.value->rgba_image.width == 2 && decoded.value->rgba_image.height == 2,
                     "single-sprite decode should use sprite dimensions");
}

int test_sprite_pair_decode_fails_for_out_of_range_index() {
  const auto image = make_multi_sprite_fixture();
  const auto palette = make_256_palette_fixture();
  const auto decoded = romulus::data::decode_caesar2_pl8_sprite_pair(image, palette, 99, true);
  if (assert_true(!decoded.ok(), "out-of-range sprite index should fail") != 0) {
    return 1;
  }

  return assert_true(decoded.error->message.find("out of range") != std::string::npos,
                     "out-of-range sprite index failure should be explicit");
}

}  // namespace

int main() {
  int rc = 0;
  rc |= test_parse_sprite_header_and_descriptor_success();
  rc |= test_decode_type0_sprite_success();
  rc |= test_parse_fails_for_truncated_descriptor_table();
  rc |= test_decode_fails_for_invalid_offset_or_size();
  rc |= test_decode_fails_for_unsupported_tile_type();
  rc |= test_decode_fails_for_rle_flag();
  rc |= test_report_format_is_stable();
  rc |= test_sprite_pair_decode_with_256_palette_success();
  rc |= test_multi_sprite_decode_and_compose_success();
  rc |= test_multi_sprite_composition_order_is_deterministic();
  rc |= test_multi_sprite_index_zero_transparency();
  rc |= test_multi_sprite_invalid_offset_fails();
  rc |= test_multi_sprite_report_format_is_stable();
  rc |= test_single_sprite_probe_report_is_stable();
  rc |= test_multi_sprite_report_shows_clean_failure_reason();
  rc |= test_sprite_pair_decode_selected_index_from_multi_sprite_fixture();
  rc |= test_sprite_pair_decode_fails_for_out_of_range_index();

  if (rc == 0) {
    std::cout << "pl8_sprite_table_resource_tests: all tests passed\n";
  }

  return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
