#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "romulus/data/binary_reader.h"

namespace romulus::data {

struct MapResource {
  static constexpr std::size_t kHeaderSize = 28;
  static constexpr std::uint16_t kMaxDimension = 4096;

  std::uint16_t format_version = 0;
  std::uint16_t header_size = 0;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::uint32_t terrain_tile_count = 0;
  std::uint32_t overlay_tile_count = 0;
  std::uint32_t flags = 0;
  std::uint32_t random_seed = 0;
  std::vector<std::uint8_t> terrain_tiles;
};

[[nodiscard]] ParseResult<MapResource> parse_caesar2_map_header(std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<MapResource> parse_caesar2_map_header(std::span<const std::uint8_t> bytes);

[[nodiscard]] ParseResult<MapResource> parse_caesar2_map(std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<MapResource> parse_caesar2_map(std::span<const std::uint8_t> bytes);

[[nodiscard]] std::string format_map_report(const MapResource& map);

}  // namespace romulus::data
