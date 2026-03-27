#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "romulus/data/binary_reader.h"
#include "romulus/data/indexed_image.h"
#include "romulus/data/pl8_resource.h"

namespace romulus::data {

struct Pl8FileHeader {
  static constexpr std::size_t kSize = 8;

  std::uint16_t flags = 0;
  std::uint16_t sprite_count = 0;
  std::uint32_t reserved = 0;

  [[nodiscard]] bool rle_encoded() const {
    return (flags & 0x0001U) != 0;
  }
};

struct Pl8SpriteDescriptor {
  static constexpr std::size_t kSize = 16;

  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::uint32_t data_offset = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::uint16_t tile_type = 0;
  std::uint16_t extra_rows = 0;
};

struct Pl8SpriteTableResource {
  static constexpr std::size_t kMinDimension = 1;
  static constexpr std::size_t kMaxDimension = 4096;

  std::size_t file_size = 0;
  Pl8FileHeader header;
  std::size_t descriptor_table_offset = 0;
  std::vector<Pl8SpriteDescriptor> sprites;
};

struct Pl8Type0SpriteDecodeResult {
  std::size_t sprite_index = 0;
  Pl8SpriteDescriptor sprite;
  std::vector<std::uint8_t> indexed_pixels;
};

struct Pl8DecodedSprite {
  std::size_t sprite_index = 0;
  Pl8SpriteDescriptor sprite;
  std::vector<std::uint8_t> indexed_pixels;
  RgbaImage rgba_image;
};

struct Pl8SpriteCompositionBounds {
  std::int32_t min_x = 0;
  std::int32_t min_y = 0;
  std::int32_t max_x = 0;
  std::int32_t max_y = 0;
};

struct Pl8SpriteCompositionResult {
  Pl8SpriteCompositionBounds bounds;
  RgbaImage rgba_image;
};

struct Pl8SpriteReportEntry {
  std::size_t sprite_index = 0;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::uint16_t tile_type = 0;
  std::uint16_t extra_rows = 0;
  std::string decode_status;
  std::string composition_status;
};

struct Pl8SpritePairMultiDecodeResult {
  Pl8SpriteTableResource image_pl8;
  Pl8Resource palette_256;
  bool decode_supported = false;
  std::vector<Pl8DecodedSprite> decoded_sprites;
  std::optional<Pl8SpriteCompositionResult> composition;
  std::vector<Pl8SpriteReportEntry> sprite_reports;
};

struct Pl8SpritePairDecodeResult {
  Pl8SpriteTableResource image_pl8;
  std::size_t sprite_index = 0;
  Pl8SpriteDescriptor sprite;
  Pl8Resource palette_256;
  RgbaImage rgba_image;
};

[[nodiscard]] ParseResult<Pl8SpriteTableResource> parse_caesar2_pl8_sprite_table(std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<Pl8SpriteTableResource> parse_caesar2_pl8_sprite_table(
    std::span<const std::uint8_t> bytes);

[[nodiscard]] ParseResult<Pl8Type0SpriteDecodeResult> decode_caesar2_pl8_type0_sprite(
    std::span<const std::uint8_t> bytes,
    std::size_t sprite_index);

[[nodiscard]] ParseResult<Pl8SpritePairDecodeResult> decode_caesar2_pl8_sprite_pair(
    std::span<const std::uint8_t> image_pl8_bytes,
    std::span<const std::uint8_t> palette_256_bytes,
    std::size_t sprite_index,
    bool index_zero_transparent = false);

[[nodiscard]] ParseResult<Pl8SpritePairMultiDecodeResult> decode_caesar2_pl8_sprite_pair_multi(
    std::span<const std::uint8_t> image_pl8_bytes,
    std::span<const std::uint8_t> palette_256_bytes,
    bool index_zero_transparent = false);

[[nodiscard]] std::string format_pl8_sprite_table_report(const Pl8SpriteTableResource& resource,
                                                         std::size_t max_sprites = 64);
[[nodiscard]] std::string format_pl8_sprite_table_report_for_sprite(const Pl8SpriteTableResource& resource,
                                                                    std::size_t sprite_index);
[[nodiscard]] std::string format_pl8_sprite_pair_multi_report(const Pl8SpritePairMultiDecodeResult& result,
                                                              std::size_t max_sprites = 64);

}  // namespace romulus::data
