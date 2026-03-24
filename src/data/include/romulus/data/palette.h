#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "romulus/data/binary_reader.h"

namespace romulus::data {

struct PaletteEntry {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
};

struct PaletteResource {
  static constexpr std::size_t kExpectedEntryCount = 256;
  static constexpr std::size_t kBytesPerEntry = 3;

  std::vector<PaletteEntry> entries;
};

[[nodiscard]] ParseResult<PaletteResource> parse_palette_resource(std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<PaletteResource> parse_palette_resource(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::string format_palette_report(const PaletteResource& palette, std::size_t max_entries = 16);

}  // namespace romulus::data
