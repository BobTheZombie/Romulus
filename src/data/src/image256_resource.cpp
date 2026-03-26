#include "romulus/data/image256_resource.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "romulus/data/palette.h"

namespace romulus::data {
namespace {

[[nodiscard]] ParseError make_invalid_256_error(std::size_t requested_bytes,
                                                 std::size_t buffer_size,
                                                 const std::string& message) {
  return make_invalid_format_error(0, requested_bytes, buffer_size, message);
}

[[nodiscard]] ParseResult<std::size_t> checked_area(std::uint16_t width, std::uint16_t height, std::size_t buffer_size) {
  if (width == 0 || height == 0) {
    return {.error = make_invalid_256_error(0, buffer_size, ".256 dimensions must be non-zero")};
  }

  if (width > IndexedImageResource::kMaxDimension || height > IndexedImageResource::kMaxDimension) {
    return {.error = make_invalid_256_error(
                0,
                buffer_size,
                ".256 dimensions exceed supported bounds (max 1024x1024)")};
  }

  const auto area = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  if (area > IndexedImageResource::kMaxPixelCount) {
    return {.error = make_invalid_256_error(
                area,
                buffer_size,
                ".256 pixel count exceeds configured decode bounds")};
  }

  return {.value = area};
}

[[nodiscard]] std::string to_upper_ascii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](const unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return text;
}

}  // namespace

ParseResult<Image256Resource> parse_caesar2_win95_raw_256(std::span<const std::byte> bytes,
                                                           const std::uint16_t width,
                                                           const std::uint16_t height) {
  const auto area = checked_area(width, height, bytes.size());
  if (!area.ok()) {
    return {.error = area.error};
  }

  if (bytes.size() != area.value.value()) {
    std::ostringstream message;
    message << ".256 payload size mismatch: expected width*height=" << area.value.value()
            << " bytes, got " << bytes.size();
    return {.error = make_invalid_256_error(area.value.value(), bytes.size(), message.str())};
  }

  Image256Resource image;
  image.width = width;
  image.height = height;
  image.payload_size = bytes.size();
  image.indexed_pixels.reserve(bytes.size());

  for (const auto byte : bytes) {
    image.indexed_pixels.push_back(std::to_integer<std::uint8_t>(byte));
  }

  return {.value = std::move(image)};
}

ParseResult<Image256Resource> parse_caesar2_win95_raw_256(std::span<const std::uint8_t> bytes,
                                                           const std::uint16_t width,
                                                           const std::uint16_t height) {
  return parse_caesar2_win95_raw_256(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()), width, height);
}

ParseResult<Image256Pl8DecodeResult> decode_caesar2_win95_256_pl8_pair(std::span<const std::uint8_t> image_bytes,
                                                                        std::span<const std::uint8_t> palette_bytes,
                                                                        const std::uint16_t width,
                                                                        const std::uint16_t height,
                                                                        const bool index_zero_transparent) {
  const auto parsed_image = parse_caesar2_win95_raw_256(image_bytes, width, height);
  if (!parsed_image.ok()) {
    return {.error = parsed_image.error};
  }

  const auto parsed_pl8 = parse_pl8_resource(palette_bytes);
  if (!parsed_pl8.ok()) {
    return {.error = parsed_pl8.error};
  }

  PaletteResource palette;
  palette.entries.reserve(parsed_pl8.value->palette_entries.size());
  for (const auto& entry : parsed_pl8.value->palette_entries) {
    if (entry.red > 63 || entry.green > 63 || entry.blue > 63) {
      return {.error = make_invalid_256_error(
                  parsed_pl8.value->payload_size,
                  parsed_pl8.value->payload_size,
                  "PL8 entry exceeds supported 6-bit component range for indexed conversion")};
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

  Image256Pl8DecodeResult decoded;
  decoded.image256 = parsed_image.value.value();
  decoded.palette_pl8 = parsed_pl8.value.value();
  decoded.rgba_image = rgba.value.value();
  return {.value = std::move(decoded)};
}

std::string format_image256_report(const Image256Resource& image, const std::size_t max_pixels) {
  std::ostringstream output;
  output << "# Caesar II Win95 .256 Report\n";
  output << "width: " << image.width << "\n";
  output << "height: " << image.height << "\n";
  output << "payload_size: " << image.payload_size << "\n";

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

std::string format_image256_pl8_report(const Image256Pl8Report& report) {
  std::ostringstream output;
  output << "# Caesar II Win95 .256+.PL8 Decode Report\n";
  output << "image_path: " << report.image256_path.generic_string() << "\n";
  output << "palette_path: " << report.palette_pl8_path.generic_string() << "\n";
  output << "width: " << report.width << "\n";
  output << "height: " << report.height << "\n";
  output << "payload_size: " << report.payload_size << "\n";
  output << "palette_entries: " << report.palette_entries << "\n";
  output << "status: " << (report.success ? "ok" : "failed") << "\n";
  output << "detail: " << report.status << "\n";
  return output.str();
}

std::optional<std::pair<std::uint16_t, std::uint16_t>> resolve_known_win95_256_dimensions(
    const std::filesystem::path& image256_path) {
  const auto upper_name = to_upper_ascii(image256_path.filename().string());
  if (upper_name == "FORUM.256") {
    return std::pair<std::uint16_t, std::uint16_t>{640, 480};
  }

  if (upper_name == "RAT_BACK.256") {
    return std::pair<std::uint16_t, std::uint16_t>{640, 480};
  }

  return std::nullopt;
}

}  // namespace romulus::data
