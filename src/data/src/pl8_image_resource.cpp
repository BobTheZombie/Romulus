#include "romulus/data/pl8_image_resource.h"

#include <algorithm>
#include <sstream>

namespace romulus::data {
namespace {

[[nodiscard]] ParseError make_invalid_pl8_image_error(std::size_t requested_bytes,
                                                       std::size_t buffer_size,
                                                       const std::string& message) {
  return make_invalid_format_error(0, requested_bytes, buffer_size, message);
}

}  // namespace

ParseResult<Pl8ImageResource> parse_caesar2_forum_pl8_image(std::span<const std::byte> bytes) {
  if (bytes.size() < Pl8ImageResource::kHeaderSize) {
    std::ostringstream message;
    message << "Unsupported FORUM-style PL8 image layout: expected at least header_size="
            << Pl8ImageResource::kHeaderSize << " bytes, got " << bytes.size();
    return {.error = make_invalid_pl8_image_error(Pl8ImageResource::kHeaderSize, bytes.size(), message.str())};
  }

  BinaryReader reader(bytes);
  if (const auto seek_prefix_error = reader.seek(Pl8ImageResource::kDimensionsOffset); seek_prefix_error.has_value()) {
    return {.error = seek_prefix_error};
  }

  const auto width_raw = reader.read_u16_le();
  if (!width_raw.ok()) {
    return {.error = width_raw.error};
  }

  const auto height_raw = reader.read_u16_le();
  if (!height_raw.ok()) {
    return {.error = height_raw.error};
  }

  if (const auto seek_suffix_error = reader.seek(Pl8ImageResource::kHeaderSize); seek_suffix_error.has_value()) {
    return {.error = seek_suffix_error};
  }

  const auto width = width_raw.value.value();
  const auto height = height_raw.value.value();
  if (width < Pl8ImageResource::kMinSupportedDimension || height < Pl8ImageResource::kMinSupportedDimension ||
      width > Pl8ImageResource::kMaxSupportedDimension || height > Pl8ImageResource::kMaxSupportedDimension) {
    std::ostringstream message;
    message << "Unsupported FORUM-style PL8 image layout: dimensions out of supported bounds (" << width << "x"
            << height << "), allowed range=[" << Pl8ImageResource::kMinSupportedDimension << ".."
            << Pl8ImageResource::kMaxSupportedDimension << "]";
    return {.error = make_invalid_pl8_image_error(bytes.size(), bytes.size(), message.str())};
  }

  const auto expected_payload_size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  const auto actual_payload_size = bytes.size() - Pl8ImageResource::kHeaderSize;
  if (actual_payload_size != expected_payload_size) {
    std::ostringstream message;
    message << "FORUM-style PL8 image payload mismatch: header width*height=" << expected_payload_size
            << " bytes, payload_bytes_after_header=" << actual_payload_size;
    return {.error = make_invalid_pl8_image_error(expected_payload_size, bytes.size(), message.str())};
  }

  Pl8ImageResource image;
  image.header_size = Pl8ImageResource::kHeaderSize;
  image.payload_offset = Pl8ImageResource::kHeaderSize;
  image.width = static_cast<std::uint16_t>(width);
  image.height = static_cast<std::uint16_t>(height);
  image.payload_size = actual_payload_size;
  image.indexed_pixels.reserve(actual_payload_size);

  for (std::size_t index = Pl8ImageResource::kHeaderSize; index < bytes.size(); ++index) {
    image.indexed_pixels.push_back(std::to_integer<std::uint8_t>(bytes[index]));
  }

  return {.value = std::move(image)};
}

ParseResult<Pl8ImageResource> parse_caesar2_forum_pl8_image(std::span<const std::uint8_t> bytes) {
  return parse_caesar2_forum_pl8_image(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
}

ParseResult<Pl8Image256PairDecodeResult> decode_caesar2_forum_pl8_image_pair(
    std::span<const std::uint8_t> image_pl8_bytes,
    std::span<const std::uint8_t> palette_256_bytes,
    const bool index_zero_transparent) {
  const auto parsed_image = parse_caesar2_forum_pl8_image(image_pl8_bytes);
  if (!parsed_image.ok()) {
    return {.error = parsed_image.error};
  }

  const auto parsed_palette = parse_pl8_resource(palette_256_bytes);
  if (!parsed_palette.ok()) {
    return {.error = parsed_palette.error};
  }

  PaletteResource palette;
  palette.entries.reserve(parsed_palette.value->palette_entries.size());
  for (const auto& entry : parsed_palette.value->palette_entries) {
    if (entry.red > 63 || entry.green > 63 || entry.blue > 63) {
      return {.error = make_invalid_pl8_image_error(
                  parsed_palette.value->payload_size,
                  parsed_palette.value->payload_size,
                  ".256 palette entry exceeds supported 6-bit component range for indexed conversion")};
    }

    palette.entries.push_back(entry);
  }

  IndexedImageResource indexed;
  indexed.width = parsed_image.value->width;
  indexed.height = parsed_image.value->height;
  indexed.indexed_pixels = parsed_image.value->indexed_pixels;

  const auto rgba = apply_palette_to_indexed_image(indexed, palette, index_zero_transparent);
  if (!rgba.ok()) {
    return {.error = rgba.error};
  }

  Pl8Image256PairDecodeResult decoded;
  decoded.image_pl8 = parsed_image.value.value();
  decoded.palette_256 = parsed_palette.value.value();
  decoded.rgba_image = rgba.value.value();
  return {.value = std::move(decoded)};
}

std::string format_pl8_image_report(const Pl8ImageResource& image, const std::size_t max_pixels) {
  std::ostringstream output;
  output << "# Caesar II Win95 FORUM-style PL8 Image Report\n";
  output << "supported_layout: forum_pl8_image_24b_header\n";
  output << "header_size: " << image.header_size << "\n";
  output << "payload_offset: " << image.payload_offset << "\n";
  output << "width: " << image.width << "\n";
  output << "height: " << image.height << "\n";
  output << "payload_size: " << image.payload_size << "\n";
  output << "payload_expected_from_dimensions: " << (static_cast<std::size_t>(image.width) * image.height) << "\n";
  output << "payload_matches_dimensions: "
         << (image.payload_size == (static_cast<std::size_t>(image.width) * image.height) ? "yes" : "no") << "\n";

  const auto count = std::min(max_pixels, image.indexed_pixels.size());
  output << "pixels:";
  for (std::size_t index = 0; index < count; ++index) {
    output << (index == 0 ? " " : ",") << static_cast<unsigned int>(image.indexed_pixels[index]);
  }
  output << "\n";

  if (count < image.indexed_pixels.size()) {
    output << "... (" << (image.indexed_pixels.size() - count) << " more pixels)\n";
  }

  return output.str();
}

}  // namespace romulus::data
