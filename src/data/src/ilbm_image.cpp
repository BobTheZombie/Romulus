#include "romulus/data/ilbm_image.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>

namespace romulus::data {
namespace {

constexpr std::size_t kFormHeaderSize = 12;
constexpr std::size_t kChunkHeaderSize = 8;
constexpr std::size_t kBmhdSize = 20;

struct ChunkView {
  std::array<char, 4> id = {};
  std::size_t header_offset = 0;
  std::size_t data_offset = 0;
  std::uint32_t size = 0;
};

[[nodiscard]] ParseError make_invalid_lbm_error(std::size_t offset,
                                                std::size_t requested_bytes,
                                                std::size_t buffer_size,
                                                const std::string& message) {
  return make_invalid_format_error(offset, requested_bytes, buffer_size, message);
}

[[nodiscard]] ParseResult<std::uint16_t> read_u16_be(BinaryReader& reader) {
  const auto b0 = reader.read_u8();
  if (!b0.ok()) {
    return {.error = b0.error};
  }

  const auto b1 = reader.read_u8();
  if (!b1.ok()) {
    return {.error = b1.error};
  }

  const auto value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(b0.value.value()) << 8U) |
                                                static_cast<std::uint16_t>(b1.value.value()));
  return {.value = value};
}

[[nodiscard]] ParseResult<std::uint32_t> read_u32_be(BinaryReader& reader) {
  const auto b0 = reader.read_u8();
  if (!b0.ok()) {
    return {.error = b0.error};
  }
  const auto b1 = reader.read_u8();
  if (!b1.ok()) {
    return {.error = b1.error};
  }
  const auto b2 = reader.read_u8();
  if (!b2.ok()) {
    return {.error = b2.error};
  }
  const auto b3 = reader.read_u8();
  if (!b3.ok()) {
    return {.error = b3.error};
  }

  const auto value = (static_cast<std::uint32_t>(b0.value.value()) << 24U) |
                     (static_cast<std::uint32_t>(b1.value.value()) << 16U) |
                     (static_cast<std::uint32_t>(b2.value.value()) << 8U) |
                     static_cast<std::uint32_t>(b3.value.value());
  return {.value = value};
}

[[nodiscard]] ParseResult<std::array<char, 4>> read_tag(BinaryReader& reader) {
  const auto tag_bytes = reader.read_bytes(4);
  if (!tag_bytes.ok()) {
    return {.error = tag_bytes.error};
  }

  std::array<char, 4> tag{};
  const auto source = tag_bytes.value.value();
  for (std::size_t i = 0; i < tag.size(); ++i) {
    tag[i] = static_cast<char>(std::to_integer<std::uint8_t>(source[i]));
  }
  return {.value = tag};
}

[[nodiscard]] bool tags_equal(const std::array<char, 4>& lhs, std::string_view rhs) {
  return rhs.size() == lhs.size() && lhs[0] == rhs[0] && lhs[1] == rhs[1] && lhs[2] == rhs[2] && lhs[3] == rhs[3];
}

[[nodiscard]] ParseResult<std::size_t> checked_pixel_count(std::uint16_t width,
                                                           std::uint16_t height,
                                                           std::size_t buffer_size) {
  if (width == 0 || height == 0) {
    return {.error = make_invalid_lbm_error(0, kBmhdSize, buffer_size, "ILBM dimensions must be non-zero")};
  }
  if (width > IlbmImageResource::kMaxDimension || height > IlbmImageResource::kMaxDimension) {
    return {.error = make_invalid_lbm_error(
                0,
                kBmhdSize,
                buffer_size,
                "ILBM dimensions exceed supported bounds (max 4096x4096)")};
  }

  const auto product = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
  if (product > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    return {.error = make_invalid_lbm_error(0, kBmhdSize, buffer_size, "ILBM pixel count exceeds host limits")};
  }
  return {.value = static_cast<std::size_t>(product)};
}

[[nodiscard]] ParseResult<std::vector<std::uint8_t>> decode_byte_run_1(std::span<const std::byte> input,
                                                                        std::size_t expected_size,
                                                                        std::size_t body_offset,
                                                                        std::size_t buffer_size) {
  BinaryReader reader(input);
  std::vector<std::uint8_t> output;
  output.reserve(expected_size);

  while (output.size() < expected_size) {
    const auto control = reader.read_u8();
    if (!control.ok()) {
      return {.error = make_invalid_lbm_error(
                  body_offset + reader.tell(),
                  1,
                  buffer_size,
                  "ByteRun1 BODY stream truncated before expected planar output")};
    }

    const auto count = static_cast<std::int8_t>(control.value.value());
    if (count >= 0) {
      const auto literal_count = static_cast<std::size_t>(count) + 1U;
      const auto literal_bytes = reader.read_bytes(literal_count);
      if (!literal_bytes.ok()) {
        return {.error = make_invalid_lbm_error(
                    body_offset + reader.tell(),
                    literal_count,
                    buffer_size,
                    "ByteRun1 BODY literal run exceeds BODY chunk bounds")};
      }
      for (const auto byte : literal_bytes.value.value()) {
        output.push_back(std::to_integer<std::uint8_t>(byte));
      }
    } else if (count != -128) {
      const auto repeated = reader.read_u8();
      if (!repeated.ok()) {
        return {.error = make_invalid_lbm_error(
                    body_offset + reader.tell(),
                    1,
                    buffer_size,
                    "ByteRun1 BODY repeat run missing payload byte")};
      }
      const auto repeat_count = static_cast<std::size_t>(1 - count);
      output.insert(output.end(), repeat_count, repeated.value.value());
    }

    if (output.size() > expected_size) {
      return {.error = make_invalid_lbm_error(
                  body_offset,
                  input.size(),
                  buffer_size,
                  "ByteRun1 BODY stream expands beyond expected planar byte count")};
    }
  }

  if (reader.remaining() != 0) {
    return {.error = make_invalid_lbm_error(
                body_offset + reader.tell(),
                reader.remaining(),
                buffer_size,
                "ByteRun1 BODY stream contains trailing compressed bytes after expected image payload")};
  }

  return {.value = std::move(output)};
}

[[nodiscard]] ParseResult<std::vector<std::uint8_t>> decode_planar_to_indexed(std::span<const std::uint8_t> planar,
                                                                               std::uint16_t width,
                                                                               std::uint16_t height,
                                                                               std::uint8_t planes,
                                                                               std::size_t buffer_size) {
  const auto row_bytes = static_cast<std::size_t>(((width + 15U) / 16U) * 2U);
  const auto expected_planar_bytes =
      row_bytes * static_cast<std::size_t>(height) * static_cast<std::size_t>(planes);
  if (planar.size() != expected_planar_bytes) {
    return {.error = make_invalid_lbm_error(
                0,
                planar.size(),
                buffer_size,
                "Decoded planar payload size does not match expected row stride * height * planes")};
  }

  const auto pixel_count = checked_pixel_count(width, height, buffer_size);
  if (!pixel_count.ok()) {
    return {.error = pixel_count.error};
  }

  std::vector<std::uint8_t> indexed(pixel_count.value.value(), 0);
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      std::uint8_t value = 0;
      for (std::size_t plane = 0; plane < planes; ++plane) {
        const auto row_plane_offset = (y * static_cast<std::size_t>(planes) + plane) * row_bytes;
        const auto byte_index = x / 8U;
        const auto bit_index = 7U - static_cast<unsigned int>(x % 8U);
        const auto source_byte = planar[row_plane_offset + byte_index];
        const auto bit = static_cast<std::uint8_t>((source_byte >> bit_index) & 0x01U);
        value |= static_cast<std::uint8_t>(bit << plane);
      }
      indexed[y * static_cast<std::size_t>(width) + x] = value;
    }
  }

  return {.value = std::move(indexed)};
}

}  // namespace

ParseResult<IlbmImageResource> parse_ilbm_image(std::span<const std::byte> bytes) {
  BinaryReader reader(bytes);

  const auto form_tag = read_tag(reader);
  if (!form_tag.ok()) {
    return {.error = form_tag.error};
  }
  if (!tags_equal(form_tag.value.value(), "FORM")) {
    return {.error = make_invalid_lbm_error(0, 4, bytes.size(), "LBM magic mismatch; expected FORM")};
  }

  const auto form_size = read_u32_be(reader);
  if (!form_size.ok()) {
    return {.error = form_size.error};
  }

  const auto form_type = read_tag(reader);
  if (!form_type.ok()) {
    return {.error = form_type.error};
  }
  if (!tags_equal(form_type.value.value(), "ILBM")) {
    return {.error = make_invalid_lbm_error(8, 4, bytes.size(), "Unsupported FORM type; expected ILBM")};
  }

  const auto expected_total = static_cast<std::uint64_t>(form_size.value.value()) + 8U;
  if (expected_total != bytes.size()) {
    std::ostringstream message;
    message << "FORM size mismatch: header declares " << expected_total << " total bytes but buffer has "
            << bytes.size();
    return {.error = make_invalid_lbm_error(4, 4, bytes.size(), message.str())};
  }

  std::optional<std::uint16_t> width;
  std::optional<std::uint16_t> height;
  std::optional<std::uint8_t> planes;
  std::optional<std::uint8_t> compression;
  std::optional<std::size_t> bmhd_offset;
  std::optional<std::size_t> cmap_offset;
  std::optional<std::size_t> body_offset;
  std::vector<PaletteEntry> palette_entries;
  std::vector<std::uint8_t> body_payload;

  while (reader.remaining() > 0) {
    const auto chunk_offset = reader.tell();
    const auto chunk_id = read_tag(reader);
    if (!chunk_id.ok()) {
      return {.error = chunk_id.error};
    }

    const auto chunk_size = read_u32_be(reader);
    if (!chunk_size.ok()) {
      return {.error = chunk_size.error};
    }

    const auto chunk_payload = reader.read_bytes(chunk_size.value.value());
    if (!chunk_payload.ok()) {
      return {.error = make_invalid_lbm_error(
                  chunk_offset + kChunkHeaderSize,
                  chunk_size.value.value(),
                  bytes.size(),
                  "Chunk payload extends beyond FORM bounds")};
    }

    if ((chunk_size.value.value() & 1U) != 0U) {
      const auto pad = reader.read_u8();
      if (!pad.ok()) {
        return {.error = make_invalid_lbm_error(
                    chunk_offset + kChunkHeaderSize + chunk_size.value.value(),
                    1,
                    bytes.size(),
                    "Missing chunk padding byte for odd-sized IFF chunk")};
      }
    }

    ChunkView chunk{
        .id = chunk_id.value.value(),
        .header_offset = chunk_offset,
        .data_offset = chunk_offset + kChunkHeaderSize,
        .size = chunk_size.value.value(),
    };

    if (tags_equal(chunk.id, "BMHD")) {
      if (bmhd_offset.has_value()) {
        return {.error = make_invalid_lbm_error(
                    chunk.header_offset,
                    kChunkHeaderSize,
                    bytes.size(),
                    "Duplicate BMHD chunk is unsupported")};
      }
      if (chunk.size != kBmhdSize) {
        return {.error = make_invalid_lbm_error(
                    chunk.data_offset,
                    chunk.size,
                    bytes.size(),
                    "BMHD chunk must be exactly 20 bytes")};
      }

      BinaryReader bmhd_reader(chunk_payload.value.value());
      const auto bmhd_width = read_u16_be(bmhd_reader);
      if (!bmhd_width.ok()) {
        return {.error = bmhd_width.error};
      }
      const auto bmhd_height = read_u16_be(bmhd_reader);
      if (!bmhd_height.ok()) {
        return {.error = bmhd_height.error};
      }
      const auto x_origin = read_u16_be(bmhd_reader);
      if (!x_origin.ok()) {
        return {.error = x_origin.error};
      }
      const auto y_origin = read_u16_be(bmhd_reader);
      if (!y_origin.ok()) {
        return {.error = y_origin.error};
      }
      const auto plane_count = bmhd_reader.read_u8();
      if (!plane_count.ok()) {
        return {.error = plane_count.error};
      }
      const auto masking = bmhd_reader.read_u8();
      if (!masking.ok()) {
        return {.error = masking.error};
      }
      const auto compression_mode = bmhd_reader.read_u8();
      if (!compression_mode.ok()) {
        return {.error = compression_mode.error};
      }
      const auto pad1 = bmhd_reader.read_u8();
      if (!pad1.ok()) {
        return {.error = pad1.error};
      }
      const auto transparent_color = read_u16_be(bmhd_reader);
      if (!transparent_color.ok()) {
        return {.error = transparent_color.error};
      }
      const auto x_aspect = bmhd_reader.read_u8();
      if (!x_aspect.ok()) {
        return {.error = x_aspect.error};
      }
      const auto y_aspect = bmhd_reader.read_u8();
      if (!y_aspect.ok()) {
        return {.error = y_aspect.error};
      }
      const auto page_width = read_u16_be(bmhd_reader);
      if (!page_width.ok()) {
        return {.error = page_width.error};
      }
      const auto page_height = read_u16_be(bmhd_reader);
      if (!page_height.ok()) {
        return {.error = page_height.error};
      }

      const auto dims_ok = checked_pixel_count(bmhd_width.value.value(), bmhd_height.value.value(), bytes.size());
      if (!dims_ok.ok()) {
        return {.error = dims_ok.error};
      }

      if (plane_count.value.value() != IlbmImageResource::kSupportedPlaneCount) {
        std::ostringstream message;
        message << "Unsupported BMHD bitplane count " << static_cast<unsigned int>(plane_count.value.value())
                << "; only 8-plane indexed ILBM is supported";
        return {.error = make_invalid_lbm_error(chunk.data_offset + 8, 1, bytes.size(), message.str())};
      }

      if (masking.value.value() != 0) {
        return {.error = make_invalid_lbm_error(
                    chunk.data_offset + 9,
                    1,
                    bytes.size(),
                    "Unsupported BMHD masking mode; only masking=0 is supported")};
      }

      if (compression_mode.value.value() != 0 && compression_mode.value.value() != 1) {
        std::ostringstream message;
        message << "Unsupported BMHD compression mode " << static_cast<unsigned int>(compression_mode.value.value())
                << "; supported modes are 0 (none) and 1 (ByteRun1)";
        return {.error = make_invalid_lbm_error(chunk.data_offset + 10, 1, bytes.size(), message.str())};
      }

      (void)x_origin;
      (void)y_origin;
      (void)pad1;
      (void)transparent_color;
      (void)x_aspect;
      (void)y_aspect;
      (void)page_width;
      (void)page_height;

      width = bmhd_width.value.value();
      height = bmhd_height.value.value();
      planes = plane_count.value.value();
      compression = compression_mode.value.value();
      bmhd_offset = chunk.header_offset;
    } else if (tags_equal(chunk.id, "CMAP")) {
      if (cmap_offset.has_value()) {
        return {.error = make_invalid_lbm_error(
                    chunk.header_offset,
                    kChunkHeaderSize,
                    bytes.size(),
                    "Duplicate CMAP chunk is unsupported")};
      }

      if ((chunk.size % 3U) != 0U) {
        return {.error = make_invalid_lbm_error(
                    chunk.data_offset,
                    chunk.size,
                    bytes.size(),
                    "CMAP chunk size must be divisible by 3")};
      }

      const auto entry_count = chunk.size / 3U;
      if (entry_count == 0U || entry_count > 256U) {
        std::ostringstream message;
        message << "CMAP chunk must contain between 1 and 256 entries; got " << entry_count;
        return {.error = make_invalid_lbm_error(chunk.data_offset, chunk.size, bytes.size(), message.str())};
      }

      BinaryReader cmap_reader(chunk_payload.value.value());
      palette_entries.clear();
      palette_entries.reserve(entry_count);
      for (std::size_t index = 0; index < entry_count; ++index) {
        const auto red = cmap_reader.read_u8();
        if (!red.ok()) {
          return {.error = red.error};
        }
        const auto green = cmap_reader.read_u8();
        if (!green.ok()) {
          return {.error = green.error};
        }
        const auto blue = cmap_reader.read_u8();
        if (!blue.ok()) {
          return {.error = blue.error};
        }
        palette_entries.push_back(PaletteEntry{
            .red = red.value.value(),
            .green = green.value.value(),
            .blue = blue.value.value(),
        });
      }

      cmap_offset = chunk.header_offset;
    } else if (tags_equal(chunk.id, "BODY")) {
      if (body_offset.has_value()) {
        return {.error = make_invalid_lbm_error(
                    chunk.header_offset,
                    kChunkHeaderSize,
                    bytes.size(),
                    "Duplicate BODY chunk is unsupported")};
      }

      body_payload.clear();
      body_payload.reserve(chunk_payload.value.value().size());
      for (const auto byte : chunk_payload.value.value()) {
        body_payload.push_back(std::to_integer<std::uint8_t>(byte));
      }
      body_offset = chunk.header_offset;
    }
  }

  if (!bmhd_offset.has_value()) {
    return {.error = make_invalid_lbm_error(0, kFormHeaderSize, bytes.size(), "Required BMHD chunk was not found")};
  }
  if (!cmap_offset.has_value()) {
    return {.error = make_invalid_lbm_error(0, kFormHeaderSize, bytes.size(), "Required CMAP chunk was not found")};
  }
  if (!body_offset.has_value()) {
    return {.error = make_invalid_lbm_error(0, kFormHeaderSize, bytes.size(), "Required BODY chunk was not found")};
  }

  const auto row_bytes = static_cast<std::size_t>(((width.value() + 15U) / 16U) * 2U);
  const auto expected_planar_bytes =
      row_bytes * static_cast<std::size_t>(height.value()) * static_cast<std::size_t>(planes.value());

  std::vector<std::uint8_t> decoded_planar;
  if (compression.value() == 0U) {
    if (body_payload.size() != expected_planar_bytes) {
      std::ostringstream message;
      message << "Uncompressed BODY size mismatch: expected " << expected_planar_bytes << " bytes but got "
              << body_payload.size();
      return {.error = make_invalid_lbm_error(body_offset.value(), body_payload.size(), bytes.size(), message.str())};
    }
    decoded_planar = body_payload;
  } else {
    const auto decoded = decode_byte_run_1(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(body_payload.data()), body_payload.size()),
        expected_planar_bytes,
        body_offset.value(),
        bytes.size());
    if (!decoded.ok()) {
      return {.error = decoded.error};
    }
    decoded_planar = std::move(decoded.value.value());
  }

  const auto indexed = decode_planar_to_indexed(decoded_planar, width.value(), height.value(), planes.value(), bytes.size());
  if (!indexed.ok()) {
    return {.error = indexed.error};
  }

  IlbmImageResource image;
  image.width = width.value();
  image.height = height.value();
  image.plane_count = planes.value();
  image.compression = compression.value();
  image.bmhd_offset = bmhd_offset.value();
  image.cmap_offset = cmap_offset.value();
  image.body_offset = body_offset.value();
  image.body_size = body_payload.size();
  image.palette_entries = std::move(palette_entries);
  image.body_payload = std::move(body_payload);
  image.indexed_pixels = std::move(indexed.value.value());
  return {.value = std::move(image)};
}

ParseResult<IlbmImageResource> parse_ilbm_image(std::span<const std::uint8_t> bytes) {
  return parse_ilbm_image(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
}

std::string format_lbm_report(const IlbmImageResource& image, std::size_t max_palette_entries) {
  std::ostringstream output;
  output << "# Caesar II Win95 LBM Report\n";
  output << "width: " << image.width << "\n";
  output << "height: " << image.height << "\n";
  output << "planes: " << static_cast<unsigned int>(image.plane_count) << "\n";
  output << "compression: " << static_cast<unsigned int>(image.compression) << "\n";
  output << "palette_entries: " << image.palette_entries.size() << "\n";
  output << "body_size: " << image.body_size << " bytes\n";
  output << "chunk_offsets: BMHD=" << image.bmhd_offset << ", CMAP=" << image.cmap_offset << ", BODY="
         << image.body_offset << "\n";
  output << "decoded_pixel_count: " << image.indexed_pixels.size() << "\n";

  const auto palette_count = std::min(max_palette_entries, image.palette_entries.size());
  output << "palette_preview:\n";
  for (std::size_t i = 0; i < palette_count; ++i) {
    const auto& entry = image.palette_entries[i];
    output << "  [" << i << "] " << static_cast<unsigned int>(entry.red) << "," << static_cast<unsigned int>(entry.green)
           << "," << static_cast<unsigned int>(entry.blue) << "\n";
  }
  if (palette_count < image.palette_entries.size()) {
    output << "  ... (" << (image.palette_entries.size() - palette_count) << " more entries)\n";
  }

  const auto pixel_count = std::min<std::size_t>(32, image.indexed_pixels.size());
  output << "pixel_preview:";
  for (std::size_t i = 0; i < pixel_count; ++i) {
    output << (i == 0 ? " " : ",") << static_cast<unsigned int>(image.indexed_pixels[i]);
  }
  output << "\n";
  if (pixel_count < image.indexed_pixels.size()) {
    output << "... (" << (image.indexed_pixels.size() - pixel_count) << " more pixels)\n";
  }

  return output.str();
}

ParseResult<RgbaImage> convert_ilbm_to_rgba(const IlbmImageResource& image) {
  const auto pixel_count = checked_pixel_count(image.width, image.height, image.indexed_pixels.size());
  if (!pixel_count.ok()) {
    return {.error = pixel_count.error};
  }

  if (image.indexed_pixels.size() != pixel_count.value.value()) {
    return {.error = make_invalid_lbm_error(
                0,
                image.indexed_pixels.size(),
                pixel_count.value.value(),
                "ILBM indexed pixel buffer size does not match image dimensions")};
  }

  if (image.palette_entries.empty()) {
    return {.error = make_invalid_lbm_error(0, 0, 0, "ILBM CMAP palette is empty")};
  }

  RgbaImage rgba;
  rgba.width = image.width;
  rgba.height = image.height;
  rgba.pixels_rgba.reserve(image.indexed_pixels.size() * 4);

  for (std::size_t index = 0; index < image.indexed_pixels.size(); ++index) {
    const auto palette_index = static_cast<std::size_t>(image.indexed_pixels[index]);
    if (palette_index >= image.palette_entries.size()) {
      std::ostringstream message;
      message << "Decoded indexed pixel at position " << index << " references missing CMAP entry "
              << palette_index << " (palette size=" << image.palette_entries.size() << ")";
      return {.error = make_invalid_lbm_error(0, image.indexed_pixels.size(), image.palette_entries.size(), message.str())};
    }

    const auto& entry = image.palette_entries[palette_index];
    rgba.pixels_rgba.push_back(entry.red);
    rgba.pixels_rgba.push_back(entry.green);
    rgba.pixels_rgba.push_back(entry.blue);
    rgba.pixels_rgba.push_back(255);
  }

  return {.value = std::move(rgba)};
}

}  // namespace romulus::data
