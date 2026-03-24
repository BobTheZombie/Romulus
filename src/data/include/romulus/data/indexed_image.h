#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "romulus/data/binary_reader.h"
#include "romulus/data/palette.h"

namespace romulus::data {

struct IndexedImageResource {
  static constexpr std::uint16_t kMaxDimension = 1024;
  static constexpr std::size_t kMaxPixelCount = static_cast<std::size_t>(kMaxDimension) * kMaxDimension;

  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::vector<std::uint8_t> indexed_pixels;
};

struct RgbaImage {
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::vector<std::uint8_t> pixels_rgba;
};

[[nodiscard]] ParseResult<IndexedImageResource> parse_caesar2_simple_indexed_tile(std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<IndexedImageResource> parse_caesar2_simple_indexed_tile(std::span<const std::uint8_t> bytes);

[[nodiscard]] ParseResult<RgbaImage> apply_palette_to_indexed_image(const IndexedImageResource& image,
                                                                    const PaletteResource& palette,
                                                                    bool index_zero_transparent = false);

[[nodiscard]] std::string format_indexed_image_report(const IndexedImageResource& image,
                                                      std::size_t max_pixels = 32);

}  // namespace romulus::data
