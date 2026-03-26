#include "romulus/data/pl8_sprite_table_resource.h"

#include <algorithm>
#include <sstream>

#include "romulus/data/palette.h"

namespace romulus::data {
namespace {

[[nodiscard]] ParseError make_invalid_sprite_table_error(std::size_t requested_bytes,
                                                          std::size_t buffer_size,
                                                          const std::string& message) {
  return make_invalid_format_error(0, requested_bytes, buffer_size, message);
}

[[nodiscard]] ParseResult<std::uint16_t> read_u16(BinaryReader& reader) {
  return reader.read_u16_le();
}

[[nodiscard]] ParseResult<std::int16_t> read_i16(BinaryReader& reader) {
  const auto raw = reader.read_u16_le();
  if (!raw.ok()) {
    return {.error = raw.error};
  }
  return {.value = static_cast<std::int16_t>(raw.value.value())};
}

[[nodiscard]] ParseResult<Pl8SpriteTableResource> parse_impl(std::span<const std::byte> bytes) {
  if (bytes.size() < Pl8FileHeader::kSize) {
    std::ostringstream message;
    message << "Unsupported PL8 sprite-table layout: expected at least header_size=" << Pl8FileHeader::kSize
            << " bytes, got " << bytes.size();
    return {.error = make_invalid_sprite_table_error(Pl8FileHeader::kSize, bytes.size(), message.str())};
  }

  BinaryReader reader(bytes);
  const auto flags = reader.read_u16_le();
  if (!flags.ok()) {
    return {.error = flags.error};
  }

  const auto sprite_count = reader.read_u16_le();
  if (!sprite_count.ok()) {
    return {.error = sprite_count.error};
  }

  const auto reserved = reader.read_u32_le();
  if (!reserved.ok()) {
    return {.error = reserved.error};
  }

  const auto descriptor_table_bytes = static_cast<std::size_t>(sprite_count.value.value()) * Pl8SpriteDescriptor::kSize;
  if (bytes.size() < Pl8FileHeader::kSize + descriptor_table_bytes) {
    std::ostringstream message;
    message << "PL8 sprite-table descriptor block truncated: header reports sprite_count="
            << sprite_count.value.value() << " requiring " << descriptor_table_bytes << " descriptor bytes";
    return {.error = make_invalid_sprite_table_error(Pl8FileHeader::kSize + descriptor_table_bytes,
                                                     bytes.size(),
                                                     message.str())};
  }

  Pl8SpriteTableResource parsed;
  parsed.file_size = bytes.size();
  parsed.descriptor_table_offset = Pl8FileHeader::kSize;
  parsed.header.flags = flags.value.value();
  parsed.header.sprite_count = sprite_count.value.value();
  parsed.header.reserved = reserved.value.value();
  parsed.sprites.reserve(parsed.header.sprite_count);

  for (std::size_t sprite_index = 0; sprite_index < parsed.header.sprite_count; ++sprite_index) {
    Pl8SpriteDescriptor descriptor;

    const auto width = read_u16(reader);
    if (!width.ok()) {
      return {.error = width.error};
    }
    descriptor.width = width.value.value();

    const auto height = read_u16(reader);
    if (!height.ok()) {
      return {.error = height.error};
    }
    descriptor.height = height.value.value();

    const auto data_offset = reader.read_u32_le();
    if (!data_offset.ok()) {
      return {.error = data_offset.error};
    }
    descriptor.data_offset = data_offset.value.value();

    const auto x = read_i16(reader);
    if (!x.ok()) {
      return {.error = x.error};
    }
    descriptor.x = x.value.value();

    const auto y = read_i16(reader);
    if (!y.ok()) {
      return {.error = y.error};
    }
    descriptor.y = y.value.value();

    const auto tile_type = read_u16(reader);
    if (!tile_type.ok()) {
      return {.error = tile_type.error};
    }
    descriptor.tile_type = tile_type.value.value();

    const auto extra_rows = read_u16(reader);
    if (!extra_rows.ok()) {
      return {.error = extra_rows.error};
    }
    descriptor.extra_rows = extra_rows.value.value();

    parsed.sprites.push_back(descriptor);
  }

  return {.value = std::move(parsed)};
}

}  // namespace

ParseResult<Pl8SpriteTableResource> parse_caesar2_pl8_sprite_table(std::span<const std::byte> bytes) {
  return parse_impl(bytes);
}

ParseResult<Pl8SpriteTableResource> parse_caesar2_pl8_sprite_table(std::span<const std::uint8_t> bytes) {
  return parse_impl(std::as_bytes(bytes));
}

ParseResult<Pl8Type0SpriteDecodeResult> decode_caesar2_pl8_type0_sprite(std::span<const std::uint8_t> bytes,
                                                                         const std::size_t sprite_index) {
  const auto parsed = parse_caesar2_pl8_sprite_table(bytes);
  if (!parsed.ok()) {
    return {.error = parsed.error};
  }

  if (sprite_index >= parsed.value->sprites.size()) {
    std::ostringstream message;
    message << "PL8 sprite index out of range: requested=" << sprite_index
            << ", sprite_count=" << parsed.value->sprites.size();
    return {.error = make_invalid_sprite_table_error(parsed.value->sprites.size(),
                                                     parsed.value->sprites.size(),
                                                     message.str())};
  }

  if (parsed.value->header.rle_encoded()) {
    return {.error = make_invalid_sprite_table_error(
                bytes.size(),
                bytes.size(),
                "Unsupported PL8 sprite-table decode: file header indicates RLE encoding")};
  }

  const auto& sprite = parsed.value->sprites[sprite_index];
  if (sprite.tile_type != 0) {
    std::ostringstream message;
    message << "Unsupported PL8 sprite tile type for decode: tile_type=" << sprite.tile_type
            << " (only type 0 is supported in this PR)";
    return {.error = make_invalid_sprite_table_error(bytes.size(), bytes.size(), message.str())};
  }

  const auto area = static_cast<std::size_t>(sprite.width) * static_cast<std::size_t>(sprite.height);
  if (sprite.width < Pl8SpriteTableResource::kMinDimension || sprite.height < Pl8SpriteTableResource::kMinDimension ||
      sprite.width > Pl8SpriteTableResource::kMaxDimension || sprite.height > Pl8SpriteTableResource::kMaxDimension) {
    std::ostringstream message;
    message << "Unsupported PL8 type-0 sprite dimensions: " << sprite.width << "x" << sprite.height;
    return {.error = make_invalid_sprite_table_error(area, bytes.size(), message.str())};
  }

  if (area > bytes.size()) {
    std::ostringstream message;
    message << "PL8 sprite decode size exceeds file bounds: required=" << area << ", file_size=" << bytes.size();
    return {.error = make_invalid_sprite_table_error(area, bytes.size(), message.str())};
  }

  const auto data_offset = static_cast<std::size_t>(sprite.data_offset);
  if (data_offset > bytes.size() || area > (bytes.size() - data_offset)) {
    std::ostringstream message;
    message << "PL8 sprite data window out of bounds: offset=" << data_offset << ", size=" << area
            << ", file_size=" << bytes.size();
    return {.error = make_invalid_sprite_table_error(data_offset + area, bytes.size(), message.str())};
  }

  Pl8Type0SpriteDecodeResult decoded;
  decoded.sprite_index = sprite_index;
  decoded.sprite = sprite;
  decoded.indexed_pixels.reserve(area);
  for (std::size_t index = 0; index < area; ++index) {
    decoded.indexed_pixels.push_back(bytes[data_offset + index]);
  }

  return {.value = std::move(decoded)};
}

ParseResult<Pl8SpritePairDecodeResult> decode_caesar2_pl8_sprite_pair(std::span<const std::uint8_t> image_pl8_bytes,
                                                                       std::span<const std::uint8_t> palette_256_bytes,
                                                                       const std::size_t sprite_index,
                                                                       const bool index_zero_transparent) {
  const auto parsed_image = parse_caesar2_pl8_sprite_table(image_pl8_bytes);
  if (!parsed_image.ok()) {
    return {.error = parsed_image.error};
  }

  const auto decoded_sprite = decode_caesar2_pl8_type0_sprite(image_pl8_bytes, sprite_index);
  if (!decoded_sprite.ok()) {
    return {.error = decoded_sprite.error};
  }

  const auto parsed_palette = parse_pl8_resource(palette_256_bytes);
  if (!parsed_palette.ok()) {
    return {.error = parsed_palette.error};
  }

  PaletteResource palette;
  palette.entries.reserve(parsed_palette.value->palette_entries.size());
  for (const auto& entry : parsed_palette.value->palette_entries) {
    if (entry.red > 63 || entry.green > 63 || entry.blue > 63) {
      return {.error = make_invalid_sprite_table_error(
                  parsed_palette.value->payload_size,
                  parsed_palette.value->payload_size,
                  ".256 palette entry exceeds supported 6-bit component range for indexed conversion")};
    }
    palette.entries.push_back(entry);
  }

  IndexedImageResource indexed;
  indexed.width = decoded_sprite.value->sprite.width;
  indexed.height = decoded_sprite.value->sprite.height;
  indexed.indexed_pixels = decoded_sprite.value->indexed_pixels;

  const auto rgba = apply_palette_to_indexed_image(indexed, palette, index_zero_transparent);
  if (!rgba.ok()) {
    return {.error = rgba.error};
  }

  Pl8SpritePairDecodeResult result;
  result.image_pl8 = parsed_image.value.value();
  result.sprite_index = sprite_index;
  result.sprite = decoded_sprite.value->sprite;
  result.palette_256 = parsed_palette.value.value();
  result.rgba_image = rgba.value.value();
  return {.value = std::move(result)};
}

std::string format_pl8_sprite_table_report(const Pl8SpriteTableResource& resource, const std::size_t max_sprites) {
  std::ostringstream output;
  output << "# Caesar II Win95 PL8 Sprite-Table Report\n";
  output << "file_size: " << resource.file_size << "\n";
  output << "header.flags: " << resource.header.flags << "\n";
  output << "header.rle: " << (resource.header.rle_encoded() ? "yes" : "no") << "\n";
  output << "header.sprite_count: " << resource.header.sprite_count << "\n";
  output << "header.reserved_u32: " << resource.header.reserved << "\n";
  output << "descriptor_table_offset: " << resource.descriptor_table_offset << "\n";

  const auto count = std::min(max_sprites, resource.sprites.size());
  for (std::size_t index = 0; index < count; ++index) {
    const auto& sprite = resource.sprites[index];
    output << "sprite[" << index << "]: width=" << sprite.width << ", height=" << sprite.height
           << ", data_offset=" << sprite.data_offset << ", x=" << sprite.x << ", y=" << sprite.y
           << ", tile_type=" << sprite.tile_type << ", extra_rows=" << sprite.extra_rows << ", decode=";

    if (resource.header.rle_encoded()) {
      output << "unsupported_rle";
    } else if (sprite.tile_type != 0) {
      output << "unsupported_tile_type";
    } else {
      const auto area = static_cast<std::size_t>(sprite.width) * static_cast<std::size_t>(sprite.height);
      const auto data_offset = static_cast<std::size_t>(sprite.data_offset);
      const bool window_valid = data_offset <= resource.file_size && area <= (resource.file_size - data_offset);
      output << (window_valid ? "supported_type0" : "invalid_offset_or_size");
    }

    output << "\n";
  }

  if (resource.sprites.size() > count) {
    output << "sprite_rows_truncated: yes\n";
    output << "sprite_rows_shown: " << count << "\n";
  }

  return output.str();
}

}  // namespace romulus::data
