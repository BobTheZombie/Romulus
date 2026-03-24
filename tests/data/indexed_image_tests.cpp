#include "romulus/data/indexed_image.h"

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

std::vector<std::uint8_t> make_tile_bytes(std::uint16_t width,
                                          std::uint16_t height,
                                          const std::vector<std::uint8_t>& pixels) {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(4 + pixels.size());

  bytes.push_back(static_cast<std::uint8_t>(width & 0xFF));
  bytes.push_back(static_cast<std::uint8_t>((width >> 8) & 0xFF));
  bytes.push_back(static_cast<std::uint8_t>(height & 0xFF));
  bytes.push_back(static_cast<std::uint8_t>((height >> 8) & 0xFF));
  bytes.insert(bytes.end(), pixels.begin(), pixels.end());

  return bytes;
}

romulus::data::PaletteResource make_identity_palette() {
  romulus::data::PaletteResource palette;
  palette.entries.reserve(romulus::data::PaletteResource::kExpectedEntryCount);

  for (std::size_t index = 0; index < romulus::data::PaletteResource::kExpectedEntryCount; ++index) {
    palette.entries.push_back(romulus::data::PaletteEntry{
        .red = static_cast<std::uint8_t>(index % 64),
        .green = static_cast<std::uint8_t>(index % 64),
        .blue = static_cast<std::uint8_t>(index % 64),
    });
  }

  return palette;
}

int test_parse_tile_success() {
  const std::vector<std::uint8_t> pixels = {0, 1, 2, 3, 4, 5};
  const auto bytes = make_tile_bytes(3, 2, pixels);

  const auto parsed = romulus::data::parse_caesar2_simple_indexed_tile(bytes);
  if (assert_true(parsed.ok(), "tile fixture should parse") != 0) {
    return 1;
  }

  const auto& image = parsed.value.value();
  if (assert_true(image.width == 3 && image.height == 2, "dimensions should decode from header") != 0) {
    return 1;
  }

  if (assert_true(image.indexed_pixels == pixels, "indexed pixels should be copied in order") != 0) {
    return 1;
  }

  return 0;
}

int test_parse_tile_rejects_truncated_payload() {
  const std::vector<std::uint8_t> pixels = {0, 1, 2};
  const auto bytes = make_tile_bytes(2, 2, pixels);

  const auto parsed = romulus::data::parse_caesar2_simple_indexed_tile(bytes);
  if (assert_true(!parsed.ok(), "truncated pixel payload should fail") != 0) {
    return 1;
  }

  if (assert_true(parsed.error.has_value() && parsed.error->code == romulus::data::ParseErrorCode::OutOfBounds,
                  "truncated payload should produce out-of-bounds parse error") != 0) {
    return 1;
  }

  return 0;
}

int test_parse_tile_rejects_trailing_bytes() {
  auto bytes = make_tile_bytes(1, 1, {7});
  bytes.push_back(9);

  const auto parsed = romulus::data::parse_caesar2_simple_indexed_tile(bytes);
  if (assert_true(!parsed.ok(), "extra bytes should fail strict decoder") != 0) {
    return 1;
  }

  if (assert_true(parsed.error.has_value() && parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                  "extra bytes should return invalid format") != 0) {
    return 1;
  }

  return 0;
}

int test_parse_tile_rejects_zero_dimensions() {
  const auto bytes = make_tile_bytes(0, 5, {});
  const auto parsed = romulus::data::parse_caesar2_simple_indexed_tile(bytes);

  if (assert_true(!parsed.ok(), "zero width should fail") != 0) {
    return 1;
  }

  if (assert_true(parsed.error.has_value() && parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                  "zero dimensions should map to invalid format") != 0) {
    return 1;
  }

  return 0;
}

int test_palette_application_success() {
  const std::vector<std::uint8_t> pixels = {0, 1, 63};
  const auto bytes = make_tile_bytes(3, 1, pixels);
  const auto parsed = romulus::data::parse_caesar2_simple_indexed_tile(bytes);

  if (assert_true(parsed.ok(), "palette test requires valid parse") != 0) {
    return 1;
  }

  const auto palette = make_identity_palette();
  const auto rgba = romulus::data::apply_palette_to_indexed_image(parsed.value.value(), palette, true);

  if (assert_true(rgba.ok(), "palette application should succeed") != 0) {
    return 1;
  }

  if (assert_true(rgba.value->pixels_rgba.size() == 12, "rgba output should have 4 bytes per pixel") != 0) {
    return 1;
  }

  if (assert_true(rgba.value->pixels_rgba[3] == 0, "index 0 should become transparent when configured") != 0) {
    return 1;
  }

  if (assert_true(rgba.value->pixels_rgba[4] == 4 && rgba.value->pixels_rgba[5] == 4 && rgba.value->pixels_rgba[6] == 4,
                  "index 1 should expand 6-bit component to 8-bit") != 0) {
    return 1;
  }

  if (assert_true(rgba.value->pixels_rgba[11] == 255, "non-zero index should keep opaque alpha") != 0) {
    return 1;
  }

  return 0;
}

int test_image_report() {
  const auto bytes = make_tile_bytes(2, 2, {1, 2, 3, 4});
  const auto parsed = romulus::data::parse_caesar2_simple_indexed_tile(bytes);

  if (assert_true(parsed.ok(), "report test requires valid parse") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_indexed_image_report(parsed.value.value(), 2);
  if (assert_true(report.find("# Caesar II Indexed Tile Report") != std::string::npos,
                  "report should include heading") != 0) {
    return 1;
  }

  if (assert_true(report.find("... (2 more pixels)") != std::string::npos,
                  "report should include truncation marker") != 0) {
    return 1;
  }

  return 0;
}

}  // namespace

int main() {
  if (test_parse_tile_success() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_tile_rejects_truncated_payload() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_tile_rejects_trailing_bytes() != 0) {
    return EXIT_FAILURE;
  }

  if (test_parse_tile_rejects_zero_dimensions() != 0) {
    return EXIT_FAILURE;
  }

  if (test_palette_application_success() != 0) {
    return EXIT_FAILURE;
  }

  if (test_image_report() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
