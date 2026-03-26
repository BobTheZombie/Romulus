#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "romulus/data/binary_reader.h"
#include "romulus/data/indexed_image.h"
#include "romulus/data/pl8_resource.h"

namespace romulus::data {

struct Image256Resource {
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::size_t payload_size = 0;
  std::vector<std::uint8_t> indexed_pixels;
};

struct Image256Pl8DecodeResult {
  Image256Resource image256;
  Pl8Resource palette_pl8;
  RgbaImage rgba_image;
};

struct Image256Pl8Report {
  std::filesystem::path image256_path;
  std::filesystem::path palette_pl8_path;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::size_t payload_size = 0;
  std::size_t palette_entries = 0;
  bool success = false;
  std::string status;
};

[[nodiscard]] ParseResult<Image256Resource> parse_caesar2_win95_raw_256(std::span<const std::byte> bytes,
                                                                         std::uint16_t width,
                                                                         std::uint16_t height);
[[nodiscard]] ParseResult<Image256Resource> parse_caesar2_win95_raw_256(std::span<const std::uint8_t> bytes,
                                                                         std::uint16_t width,
                                                                         std::uint16_t height);

[[nodiscard]] ParseResult<Image256Pl8DecodeResult> decode_caesar2_win95_256_pl8_pair(
    std::span<const std::uint8_t> image_bytes,
    std::span<const std::uint8_t> palette_bytes,
    std::uint16_t width,
    std::uint16_t height,
    bool index_zero_transparent = false);

[[nodiscard]] std::string format_image256_report(const Image256Resource& image, std::size_t max_pixels = 32);
[[nodiscard]] std::string format_image256_pl8_report(const Image256Pl8Report& report);

[[nodiscard]] std::optional<std::pair<std::uint16_t, std::uint16_t>> resolve_known_win95_256_dimensions(
    const std::filesystem::path& image256_path);

}  // namespace romulus::data
