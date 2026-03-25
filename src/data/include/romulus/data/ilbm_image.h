#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "romulus/data/binary_reader.h"
#include "romulus/data/indexed_image.h"
#include "romulus/data/palette.h"

namespace romulus::data {

struct IlbmImageResource {
  static constexpr std::uint16_t kMaxDimension = 4096;
  static constexpr std::uint8_t kSupportedPlaneCount = 8;

  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::uint8_t plane_count = 0;
  std::uint8_t compression = 0;
  std::size_t bmhd_offset = 0;
  std::size_t cmap_offset = 0;
  std::size_t body_offset = 0;
  std::size_t body_size = 0;
  std::vector<PaletteEntry> palette_entries;
  std::vector<std::uint8_t> body_payload;
  std::vector<std::uint8_t> indexed_pixels;
};

[[nodiscard]] ParseResult<IlbmImageResource> parse_ilbm_image(std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<IlbmImageResource> parse_ilbm_image(std::span<const std::uint8_t> bytes);

[[nodiscard]] ParseResult<RgbaImage> convert_ilbm_to_rgba(const IlbmImageResource& image);

[[nodiscard]] std::string format_lbm_report(const IlbmImageResource& image, std::size_t max_palette_entries = 16);

}  // namespace romulus::data
