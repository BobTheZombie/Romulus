#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "romulus/data/binary_reader.h"
#include "romulus/data/indexed_image.h"
#include "romulus/data/palette.h"
#include "romulus/data/pl8_resource.h"

namespace romulus::data {

struct Pl8ImageResource {
  static constexpr std::size_t kHeaderSize = 24;
  static constexpr std::uint16_t kSupportedWidth = 640;
  static constexpr std::uint16_t kSupportedHeight = 480;
  static constexpr std::size_t kSupportedPayloadSize =
      static_cast<std::size_t>(kSupportedWidth) * static_cast<std::size_t>(kSupportedHeight);
  static constexpr std::size_t kSupportedFileSize = kHeaderSize + kSupportedPayloadSize;

  std::size_t header_size = 0;
  std::size_t payload_offset = 0;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::size_t payload_size = 0;
  std::vector<std::uint8_t> indexed_pixels;
};

struct Pl8Image256PairDecodeResult {
  Pl8ImageResource image_pl8;
  Pl8Resource palette_256;
  RgbaImage rgba_image;
};

[[nodiscard]] ParseResult<Pl8ImageResource> parse_caesar2_forum_pl8_image(std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<Pl8ImageResource> parse_caesar2_forum_pl8_image(std::span<const std::uint8_t> bytes);

[[nodiscard]] ParseResult<Pl8Image256PairDecodeResult> decode_caesar2_forum_pl8_image_pair(
    std::span<const std::uint8_t> image_pl8_bytes,
    std::span<const std::uint8_t> palette_256_bytes,
    bool index_zero_transparent = false);

[[nodiscard]] std::string format_pl8_image_report(const Pl8ImageResource& image, std::size_t max_pixels = 32);

}  // namespace romulus::data
