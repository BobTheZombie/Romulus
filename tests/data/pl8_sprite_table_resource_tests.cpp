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

  if (rc == 0) {
    std::cout << "pl8_sprite_table_resource_tests: all tests passed\n";
  }

  return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
