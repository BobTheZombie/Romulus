#include "romulus/data/indexed_image.h"

#include <algorithm>
#include <sstream>

namespace romulus::data {
namespace {

constexpr std::size_t kHeaderBytes = sizeof(std::uint16_t) + sizeof(std::uint16_t);

[[nodiscard]] ParseError make_invalid_tile_error(std::size_t offset,
                                                 std::size_t requested_bytes,
                                                 std::size_t buffer_size,
                                                 const std::string& message) {
  return make_invalid_format_error(offset, requested_bytes, buffer_size, message);
}

[[nodiscard]] ParseResult<std::size_t> checked_area(std::uint16_t width, std::uint16_t height, std::size_t buffer_size) {
  if (width == 0 || height == 0) {
    return {.error = make_invalid_tile_error(0, kHeaderBytes, buffer_size, "Tile dimensions must be non-zero")};
  }

  if (width > IndexedImageResource::kMaxDimension || height > IndexedImageResource::kMaxDimension) {
    return {.error = make_invalid_tile_error(
                0,
                kHeaderBytes,
                buffer_size,
                "Tile dimensions exceed supported bounds (max 1024x1024)")};
  }

  const auto area = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  if (area > IndexedImageResource::kMaxPixelCount) {
    return {.error = make_invalid_tile_error(
                0,
                kHeaderBytes,
                buffer_size,
                "Tile pixel count exceeds configured decode bounds")};
  }

  return {.value = area};
}

[[nodiscard]] std::uint8_t expand_6bit_to_8bit(std::uint8_t component) {
  return static_cast<std::uint8_t>((static_cast<std::uint16_t>(component) * 255U) / 63U);
}

}  // namespace

ParseResult<IndexedImageResource> parse_caesar2_simple_indexed_tile(std::span<const std::byte> bytes) {
  BinaryReader reader(bytes);

  const auto width = reader.read_u16_le();
  if (!width.ok()) {
    return {.error = width.error};
  }

  const auto height = reader.read_u16_le();
  if (!height.ok()) {
    return {.error = height.error};
  }

  const auto area = checked_area(width.value.value(), height.value.value(), bytes.size());
  if (!area.ok()) {
    return {.error = area.error};
  }

  const auto pixel_bytes = reader.read_bytes(area.value.value());
  if (!pixel_bytes.ok()) {
    return {.error = pixel_bytes.error};
  }

  if (reader.remaining() != 0) {
    return {.error = make_invalid_tile_error(reader.tell(), reader.remaining(), bytes.size(), "Unexpected trailing bytes after indexed pixels")};
  }

  IndexedImageResource image;
  image.width = width.value.value();
  image.height = height.value.value();
  image.indexed_pixels.reserve(area.value.value());

  for (const auto pixel : pixel_bytes.value.value()) {
    image.indexed_pixels.push_back(std::to_integer<std::uint8_t>(pixel));
  }

  return {.value = std::move(image)};
}

ParseResult<IndexedImageResource> parse_caesar2_simple_indexed_tile(std::span<const std::uint8_t> bytes) {
  return parse_caesar2_simple_indexed_tile(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
}

ParseResult<RgbaImage> apply_palette_to_indexed_image(const IndexedImageResource& image,
                                                       const PaletteResource& palette,
                                                       bool index_zero_transparent) {
  if (palette.entries.size() < PaletteResource::kExpectedEntryCount) {
    return {.error = make_invalid_tile_error(
                0,
                0,
                palette.entries.size(),
                "Palette must contain 256 entries for indexed conversion")};
  }

  const auto area = checked_area(image.width, image.height, image.indexed_pixels.size());
  if (!area.ok()) {
    return {.error = area.error};
  }

  if (area.value.value() != image.indexed_pixels.size()) {
    return {.error = make_invalid_tile_error(
                0,
                image.indexed_pixels.size(),
                area.value.value(),
                "Indexed pixel vector size does not match image dimensions")};
  }

  RgbaImage rgba;
  rgba.width = image.width;
  rgba.height = image.height;
  rgba.pixels_rgba.reserve(image.indexed_pixels.size() * 4);

  for (const auto index : image.indexed_pixels) {
    const auto& entry = palette.entries[index];
    rgba.pixels_rgba.push_back(expand_6bit_to_8bit(entry.red));
    rgba.pixels_rgba.push_back(expand_6bit_to_8bit(entry.green));
    rgba.pixels_rgba.push_back(expand_6bit_to_8bit(entry.blue));
    rgba.pixels_rgba.push_back(static_cast<std::uint8_t>(index_zero_transparent && index == 0 ? 0 : 255));
  }

  return {.value = std::move(rgba)};
}

std::string format_indexed_image_report(const IndexedImageResource& image, std::size_t max_pixels) {
  std::ostringstream output;
  output << "# Caesar II Indexed Tile Report\n";
  output << "width: " << image.width << "\n";
  output << "height: " << image.height << "\n";
  output << "pixel_count: " << image.indexed_pixels.size() << "\n";

  const auto count = std::min(max_pixels, image.indexed_pixels.size());
  output << "pixels:";
  for (std::size_t index = 0; index < count; ++index) {
    output << (index == 0 ? " " : ",") << static_cast<int>(image.indexed_pixels[index]);
  }
  output << "\n";

  if (count < image.indexed_pixels.size()) {
    output << "... (" << (image.indexed_pixels.size() - count) << " more pixels)\n";
  }

  return output.str();
}

}  // namespace romulus::data
