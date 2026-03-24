#include "romulus/data/map_resource.h"

#include <array>
#include <limits>
#include <sstream>

namespace romulus::data {
namespace {

[[nodiscard]] ParseError make_invalid_map_error(std::size_t offset,
                                                std::size_t requested_bytes,
                                                std::size_t buffer_size,
                                                const std::string& message) {
  return make_invalid_format_error(offset, requested_bytes, buffer_size, message);
}

[[nodiscard]] ParseResult<std::size_t> checked_tile_count(std::uint16_t width,
                                                          std::uint16_t height,
                                                          std::size_t buffer_size) {
  if (width == 0 || height == 0) {
    return {.error = make_invalid_map_error(8, 4, buffer_size, "Map dimensions must be non-zero")};
  }

  if (width > MapResource::kMaxDimension || height > MapResource::kMaxDimension) {
    return {.error = make_invalid_map_error(
                8,
                4,
                buffer_size,
                "Map dimensions exceed supported bounds (max 4096x4096)")};
  }

  const auto width_wide = static_cast<std::uint64_t>(width);
  const auto height_wide = static_cast<std::uint64_t>(height);
  const auto count_wide = width_wide * height_wide;

  if (count_wide > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    return {.error = make_invalid_map_error(8, 4, buffer_size, "Map tile count exceeds host size limits")};
  }

  return {.value = static_cast<std::size_t>(count_wide)};
}

[[nodiscard]] ParseResult<std::size_t> checked_payload_size(std::uint32_t terrain_tile_count,
                                                            std::size_t buffer_size) {
  const auto count_wide = static_cast<std::uint64_t>(terrain_tile_count);
  constexpr std::uint64_t kTileByteSize = sizeof(std::uint8_t);
  const auto payload_wide = count_wide * kTileByteSize;

  if (payload_wide > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    return {.error = make_invalid_map_error(
                MapResource::kHeaderSize,
                sizeof(std::uint8_t),
                buffer_size,
                "Terrain payload size exceeds host size limits")};
  }

  return {.value = static_cast<std::size_t>(payload_wide)};
}

}  // namespace

ParseResult<MapResource> parse_caesar2_map_header(std::span<const std::byte> bytes) {
  BinaryReader reader(bytes);

  const auto magic_bytes = reader.read_bytes(4);
  if (!magic_bytes.ok()) {
    return {.error = magic_bytes.error};
  }

  constexpr std::array<std::byte, 4> kKnownMagic = {
      std::byte{'M'},
      std::byte{'A'},
      std::byte{'P'},
      std::byte{'0'},
  };

  const auto magic = magic_bytes.value.value();
  if (magic.size() != kKnownMagic.size() || magic[0] != kKnownMagic[0] || magic[1] != kKnownMagic[1] ||
      magic[2] != kKnownMagic[2] || magic[3] != kKnownMagic[3]) {
    return {.error = make_invalid_map_error(0, 4, bytes.size(), "Map header magic mismatch; expected MAP0")};
  }

  const auto format_version = reader.read_u16_le();
  if (!format_version.ok()) {
    return {.error = format_version.error};
  }

  const auto header_size = reader.read_u16_le();
  if (!header_size.ok()) {
    return {.error = header_size.error};
  }

  if (header_size.value.value() != MapResource::kHeaderSize) {
    std::ostringstream message;
    message << "Unsupported map header size " << header_size.value.value() << "; expected "
            << MapResource::kHeaderSize;

    return {.error = make_invalid_map_error(6, 2, bytes.size(), message.str())};
  }

  const auto width = reader.read_u16_le();
  if (!width.ok()) {
    return {.error = width.error};
  }

  const auto height = reader.read_u16_le();
  if (!height.ok()) {
    return {.error = height.error};
  }

  const auto terrain_tile_count = reader.read_u32_le();
  if (!terrain_tile_count.ok()) {
    return {.error = terrain_tile_count.error};
  }

  const auto overlay_tile_count = reader.read_u32_le();
  if (!overlay_tile_count.ok()) {
    return {.error = overlay_tile_count.error};
  }

  const auto flags = reader.read_u32_le();
  if (!flags.ok()) {
    return {.error = flags.error};
  }

  const auto random_seed = reader.read_u32_le();
  if (!random_seed.ok()) {
    return {.error = random_seed.error};
  }

  const auto expected_tile_count = checked_tile_count(width.value.value(), height.value.value(), bytes.size());
  if (!expected_tile_count.ok()) {
    return {.error = expected_tile_count.error};
  }

  if (terrain_tile_count.value.value() != expected_tile_count.value.value()) {
    std::ostringstream message;
    message << "Terrain tile count mismatch: header=" << terrain_tile_count.value.value()
            << ", expected width*height=" << expected_tile_count.value.value();

    return {.error = make_invalid_map_error(12, 4, bytes.size(), message.str())};
  }

  if (overlay_tile_count.value.value() > terrain_tile_count.value.value()) {
    return {.error = make_invalid_map_error(
                16,
                4,
                bytes.size(),
                "Overlay tile count must not exceed terrain tile count")};
  }

  MapResource map;
  map.format_version = format_version.value.value();
  map.header_size = header_size.value.value();
  map.width = width.value.value();
  map.height = height.value.value();
  map.terrain_tile_count = terrain_tile_count.value.value();
  map.overlay_tile_count = overlay_tile_count.value.value();
  map.flags = flags.value.value();
  map.random_seed = random_seed.value.value();

  return {.value = map};
}

ParseResult<MapResource> parse_caesar2_map_header(std::span<const std::uint8_t> bytes) {
  return parse_caesar2_map_header(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
}

ParseResult<MapResource> parse_caesar2_map(std::span<const std::byte> bytes) {
  const auto parsed_header = parse_caesar2_map_header(bytes);
  if (!parsed_header.ok()) {
    return {.error = parsed_header.error};
  }

  MapResource map = parsed_header.value.value();

  const auto payload_size = checked_payload_size(map.terrain_tile_count, bytes.size());
  if (!payload_size.ok()) {
    return {.error = payload_size.error};
  }

  BinaryReader reader(bytes);
  const auto seek_error = reader.seek(MapResource::kHeaderSize);
  if (seek_error.has_value()) {
    return {.error = seek_error};
  }

  const auto terrain_bytes = reader.read_bytes(payload_size.value.value());
  if (!terrain_bytes.ok()) {
    return {.error = terrain_bytes.error};
  }

  const auto source = terrain_bytes.value.value();
  map.terrain_tiles.reserve(source.size());
  for (const auto byte : source) {
    map.terrain_tiles.push_back(std::to_integer<std::uint8_t>(byte));
  }

  return {.value = map};
}

ParseResult<MapResource> parse_caesar2_map(std::span<const std::uint8_t> bytes) {
  return parse_caesar2_map(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
}

std::string format_map_report(const MapResource& map) {
  std::ostringstream output;
  output << "# Caesar II Map Report\n";
  output << "format_version: " << map.format_version << "\n";
  output << "header_size: " << map.header_size << "\n";
  output << "width: " << map.width << "\n";
  output << "height: " << map.height << "\n";
  output << "terrain_tile_count: " << map.terrain_tile_count << "\n";
  output << "overlay_tile_count: " << map.overlay_tile_count << "\n";
  output << "flags: 0x" << std::hex << map.flags << std::dec << "\n";
  output << "random_seed: " << map.random_seed << "\n";
  output << "terrain_payload_decoded: " << map.terrain_tiles.size() << " bytes\n";

  constexpr std::size_t kSampleSize = 8;
  const auto sample_count = map.terrain_tiles.size() < kSampleSize ? map.terrain_tiles.size() : kSampleSize;
  output << "terrain_sample[0..";
  if (sample_count == 0) {
    output << "-1]: (empty)\n";
    return output.str();
  }

  output << (sample_count - 1) << "]: ";
  for (std::size_t i = 0; i < sample_count; ++i) {
    if (i > 0) {
      output << ", ";
    }
    output << static_cast<unsigned int>(map.terrain_tiles[i]);
  }
  output << "\n";

  return output.str();
}

}  // namespace romulus::data
