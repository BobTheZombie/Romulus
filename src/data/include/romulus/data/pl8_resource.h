#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "romulus/data/binary_reader.h"
#include "romulus/data/palette.h"

namespace romulus::data {

struct Pl8Resource {
  static constexpr std::size_t kSupportedEntryCount = 256;
  static constexpr std::size_t kBytesPerEntry = 3;
  static constexpr std::size_t kSupportedPayloadSize = kSupportedEntryCount * kBytesPerEntry;

  std::size_t payload_offset = 0;
  std::size_t payload_size = 0;
  std::vector<PaletteEntry> palette_entries;
};

[[nodiscard]] ParseResult<Pl8Resource> parse_pl8_resource(std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<Pl8Resource> parse_pl8_resource(std::span<const std::uint8_t> bytes);

[[nodiscard]] std::string format_pl8_report(const Pl8Resource& resource, std::size_t max_palette_entries = 16);

}  // namespace romulus::data
