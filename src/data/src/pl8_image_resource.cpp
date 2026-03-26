#include "romulus/data/pl8_image_resource.h"

#include <algorithm>
#include <array>
#include <map>
#include <sstream>

namespace romulus::data {
namespace {

struct ParsedLargePl8Header {
  std::uint16_t width = 0;
  std::uint16_t height = 0;
};

struct DecodedRgbaFromIndexed {
  Pl8Resource palette_256;
  RgbaImage rgba_image;
};

[[nodiscard]] ParseError make_invalid_pl8_image_error(std::size_t requested_bytes,
                                                       std::size_t buffer_size,
                                                       const std::string& message) {
  return make_invalid_format_error(0, requested_bytes, buffer_size, message);
}

[[nodiscard]] ParseResult<ParsedLargePl8Header> parse_large_pl8_common_header(std::span<const std::byte> bytes) {
  if (bytes.size() < Pl8ImageResource::kHeaderSize) {
    std::ostringstream message;
    message << "Unsupported large-PL8 image layout: expected at least header_size=" << Pl8ImageResource::kHeaderSize
            << " bytes, got " << bytes.size();
    return {.error = make_invalid_pl8_image_error(Pl8ImageResource::kHeaderSize, bytes.size(), message.str())};
  }

  BinaryReader reader(bytes);
  if (const auto seek_prefix_error = reader.seek(Pl8ImageResource::kDimensionsOffset); seek_prefix_error.has_value()) {
    return {.error = seek_prefix_error};
  }

  const auto width_raw = reader.read_u16_le();
  if (!width_raw.ok()) {
    return {.error = width_raw.error};
  }

  const auto height_raw = reader.read_u16_le();
  if (!height_raw.ok()) {
    return {.error = height_raw.error};
  }

  ParsedLargePl8Header parsed;
  parsed.width = width_raw.value.value();
  parsed.height = height_raw.value.value();
  return {.value = parsed};
}

[[nodiscard]] std::string format_preview_bytes(std::span<const std::uint8_t> bytes) {
  if (bytes.empty()) {
    return "(none)";
  }

  std::ostringstream output;
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    output << (index == 0 ? "" : ",") << static_cast<unsigned int>(bytes[index]);
  }
  return output.str();
}

void copy_preview(const std::span<const std::byte> bytes,
                  const std::size_t offset,
                  const std::size_t count,
                  std::vector<std::uint8_t>& output) {
  output.clear();
  if (count == 0) {
    return;
  }

  output.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    output.push_back(std::to_integer<std::uint8_t>(bytes[offset + index]));
  }
}

[[nodiscard]] ParseResult<DecodedRgbaFromIndexed> decode_indexed_with_256_palette(
    const std::vector<std::uint8_t>& indexed_pixels,
    const std::uint16_t width,
    const std::uint16_t height,
    std::span<const std::uint8_t> palette_256_bytes,
    const bool index_zero_transparent) {
  const auto parsed_palette = parse_pl8_resource(palette_256_bytes);
  if (!parsed_palette.ok()) {
    return {.error = parsed_palette.error};
  }

  PaletteResource palette;
  palette.entries.reserve(parsed_palette.value->palette_entries.size());
  for (const auto& entry : parsed_palette.value->palette_entries) {
    if (entry.red > 63 || entry.green > 63 || entry.blue > 63) {
      return {.error = make_invalid_pl8_image_error(
                  parsed_palette.value->payload_size,
                  parsed_palette.value->payload_size,
                  ".256 palette entry exceeds supported 6-bit component range for indexed conversion")};
    }

    palette.entries.push_back(entry);
  }

  IndexedImageResource indexed;
  indexed.width = width;
  indexed.height = height;
  indexed.indexed_pixels = indexed_pixels;

  const auto rgba = apply_palette_to_indexed_image(indexed, palette, index_zero_transparent);
  if (!rgba.ok()) {
    return {.error = rgba.error};
  }

  DecodedRgbaFromIndexed decoded;
  decoded.palette_256 = parsed_palette.value.value();
  decoded.rgba_image = rgba.value.value();
  return {.value = std::move(decoded)};
}

}  // namespace

ParseResult<Pl8ImageResource> parse_caesar2_forum_pl8_image(std::span<const std::byte> bytes) {
  const auto parsed_header = parse_large_pl8_common_header(bytes);
  if (!parsed_header.ok()) {
    if (parsed_header.error->message.find("Unsupported large-PL8 image layout") != std::string::npos) {
      std::ostringstream message;
      message << "Unsupported FORUM-style PL8 image layout: expected at least header_size="
              << Pl8ImageResource::kHeaderSize << " bytes, got " << bytes.size();
      return {.error = make_invalid_pl8_image_error(Pl8ImageResource::kHeaderSize, bytes.size(), message.str())};
    }
    return {.error = parsed_header.error};
  }

  const auto width = parsed_header.value->width;
  const auto height = parsed_header.value->height;
  if (width < Pl8ImageResource::kMinSupportedDimension || height < Pl8ImageResource::kMinSupportedDimension ||
      width > Pl8ImageResource::kMaxSupportedDimension || height > Pl8ImageResource::kMaxSupportedDimension) {
    std::ostringstream message;
    message << "Unsupported FORUM-style PL8 image layout: dimensions out of supported bounds (" << width << "x"
            << height << "), allowed range=[" << Pl8ImageResource::kMinSupportedDimension << ".."
            << Pl8ImageResource::kMaxSupportedDimension << "]";
    return {.error = make_invalid_pl8_image_error(bytes.size(), bytes.size(), message.str())};
  }

  const auto expected_payload_size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  const auto actual_payload_size = bytes.size() - Pl8ImageResource::kHeaderSize;
  if (actual_payload_size != expected_payload_size) {
    std::ostringstream message;
    message << "FORUM-style PL8 image payload mismatch: header width*height=" << expected_payload_size
            << " bytes, payload_bytes_after_header=" << actual_payload_size;
    return {.error = make_invalid_pl8_image_error(expected_payload_size, bytes.size(), message.str())};
  }

  Pl8ImageResource image;
  image.header_size = Pl8ImageResource::kHeaderSize;
  image.payload_offset = Pl8ImageResource::kHeaderSize;
  image.width = static_cast<std::uint16_t>(width);
  image.height = static_cast<std::uint16_t>(height);
  image.payload_size = actual_payload_size;
  image.indexed_pixels.reserve(actual_payload_size);

  for (std::size_t index = Pl8ImageResource::kHeaderSize; index < bytes.size(); ++index) {
    image.indexed_pixels.push_back(std::to_integer<std::uint8_t>(bytes[index]));
  }

  return {.value = std::move(image)};
}

ParseResult<Pl8ImageResource> parse_caesar2_forum_pl8_image(std::span<const std::uint8_t> bytes) {
  return parse_caesar2_forum_pl8_image(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
}

ParseResult<StructuredPl8Resource> parse_caesar2_rat_back_structured_pl8_image(std::span<const std::byte> bytes) {
  const auto parsed_header = parse_large_pl8_common_header(bytes);
  if (!parsed_header.ok()) {
    if (parsed_header.error->message.find("Unsupported large-PL8 image layout") != std::string::npos) {
      std::ostringstream message;
      message << "Unsupported RAT_BACK-style structured PL8 image layout: expected at least header_size="
              << Pl8ImageResource::kHeaderSize << " bytes, got " << bytes.size();
      return {.error = make_invalid_pl8_image_error(Pl8ImageResource::kHeaderSize, bytes.size(), message.str())};
    }
    return {.error = parsed_header.error};
  }

  const auto width = parsed_header.value->width;
  const auto height = parsed_header.value->height;
  if (width < Pl8ImageResource::kMinSupportedDimension || height < Pl8ImageResource::kMinSupportedDimension ||
      width > Pl8ImageResource::kMaxSupportedDimension || height > Pl8ImageResource::kMaxSupportedDimension) {
    std::ostringstream message;
    message << "Unsupported RAT_BACK-style structured PL8 image layout: dimensions out of supported bounds (" << width
            << "x" << height << "), allowed range=[" << Pl8ImageResource::kMinSupportedDimension << ".."
            << Pl8ImageResource::kMaxSupportedDimension << "]";
    return {.error = make_invalid_pl8_image_error(bytes.size(), bytes.size(), message.str())};
  }

  const auto payload_size = bytes.size() - Pl8ImageResource::kHeaderSize;
  if (payload_size < StructuredPl8Resource::kStructuredPrefixSize) {
    std::ostringstream message;
    message << "RAT_BACK-style structured payload too small: requires prefix_size="
            << StructuredPl8Resource::kStructuredPrefixSize << " bytes, payload_bytes_after_header=" << payload_size;
    return {.error = make_invalid_pl8_image_error(StructuredPl8Resource::kStructuredPrefixSize, payload_size, message.str())};
  }

  StructuredPl8Resource parsed;
  parsed.header_size = Pl8ImageResource::kHeaderSize;
  parsed.payload_offset = Pl8ImageResource::kHeaderSize;
  parsed.width = width;
  parsed.height = height;
  parsed.payload_size = payload_size;
  parsed.expected_image_size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

  BinaryReader reader(bytes);
  if (const auto seek_prefix_error = reader.seek(parsed.payload_offset); seek_prefix_error.has_value()) {
    return {.error = seek_prefix_error};
  }

  constexpr std::size_t kPrefixFieldCount = StructuredPl8Resource::kStructuredPrefixSize / sizeof(std::uint32_t);
  parsed.prefix_fields.reserve(kPrefixFieldCount);
  for (std::size_t field_index = 0; field_index < kPrefixFieldCount; ++field_index) {
    const auto field = reader.read_u32_le();
    if (!field.ok()) {
      return {.error = field.error};
    }

    parsed.prefix_fields.push_back({.index = field_index, .value = field.value.value()});
  }

  const auto bounded_payload_start = parsed.payload_offset + StructuredPl8Resource::kStructuredPrefixSize;
  const auto bounded_payload_size = parsed.payload_size - StructuredPl8Resource::kStructuredPrefixSize;
  for (std::size_t offset_field_index = 0; offset_field_index < parsed.prefix_fields.size(); ++offset_field_index) {
    for (std::size_t size_field_index = 0; size_field_index < parsed.prefix_fields.size(); ++size_field_index) {
      StructuredPl8CandidateRegion candidate;
      candidate.offset_field_index = offset_field_index;
      candidate.size_field_index = size_field_index;
      candidate.payload_offset = static_cast<std::size_t>(parsed.prefix_fields[offset_field_index].value);
      candidate.payload_size = static_cast<std::size_t>(parsed.prefix_fields[size_field_index].value);

      if (candidate.payload_size != parsed.expected_image_size) {
        candidate.reason = "size_field_does_not_match_width_times_height";
        parsed.candidate_regions.push_back(std::move(candidate));
        continue;
      }

      if (candidate.payload_offset < StructuredPl8Resource::kStructuredPrefixSize) {
        candidate.reason = "offset_points_into_structured_prefix";
        parsed.candidate_regions.push_back(std::move(candidate));
        continue;
      }

      if (candidate.payload_offset > parsed.payload_size) {
        candidate.reason = "offset_out_of_payload_bounds";
        parsed.candidate_regions.push_back(std::move(candidate));
        continue;
      }

      if (candidate.payload_size > parsed.payload_size || candidate.payload_offset + candidate.payload_size > parsed.payload_size) {
        candidate.reason = "candidate_range_out_of_payload_bounds";
        parsed.candidate_regions.push_back(std::move(candidate));
        continue;
      }

      const auto absolute_offset = parsed.payload_offset + candidate.payload_offset;
      if (absolute_offset < bounded_payload_start ||
          absolute_offset + candidate.payload_size > bounded_payload_start + bounded_payload_size) {
        candidate.reason = "candidate_range_violates_bounded_structured_region";
        parsed.candidate_regions.push_back(std::move(candidate));
        continue;
      }

      candidate.accepted = true;
      candidate.reason = "accepted";
      parsed.candidate_regions.push_back(std::move(candidate));
    }
  }

  std::vector<StructuredPl8CandidateRegion> accepted_candidates;
  for (const auto& candidate : parsed.candidate_regions) {
    if (candidate.accepted) {
      accepted_candidates.push_back(candidate);
    }
  }

  if (accepted_candidates.empty()) {
    std::ostringstream message;
    message << "RAT_BACK-style structured PL8 did not yield a deterministic image region: expected exactly one accepted "
               "candidate with size=width*height ("
            << parsed.expected_image_size << "), got accepted_candidates=0";
    return {.error = make_invalid_pl8_image_error(parsed.expected_image_size, parsed.payload_size, message.str())};
  }

  if (accepted_candidates.size() != 1) {
    std::ostringstream message;
    message << "RAT_BACK-style structured PL8 ambiguous image region: accepted_candidates=" << accepted_candidates.size()
            << " (requires exactly 1 deterministic candidate)";
    return {.error = make_invalid_pl8_image_error(parsed.expected_image_size, parsed.payload_size, message.str())};
  }

  parsed.selected_region = accepted_candidates.front();
  parsed.indexed_pixels.reserve(parsed.expected_image_size);
  const auto absolute_offset = parsed.payload_offset + parsed.selected_region->payload_offset;
  for (std::size_t index = 0; index < parsed.expected_image_size; ++index) {
    parsed.indexed_pixels.push_back(std::to_integer<std::uint8_t>(bytes[absolute_offset + index]));
  }

  return {.value = std::move(parsed)};
}

ParseResult<StructuredPl8Resource> parse_caesar2_rat_back_structured_pl8_image(std::span<const std::uint8_t> bytes) {
  return parse_caesar2_rat_back_structured_pl8_image(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
}

ParseResult<Pl8ImageVariantProbeReport> probe_caesar2_large_pl8_image_variant(std::span<const std::byte> bytes) {
  const auto parsed_header = parse_large_pl8_common_header(bytes);
  if (!parsed_header.ok()) {
    return {.error = parsed_header.error};
  }

  Pl8ImageVariantProbeReport report;
  report.file_size = bytes.size();
  report.header_size = Pl8ImageResource::kHeaderSize;
  report.payload_offset = Pl8ImageResource::kHeaderSize;
  report.width = parsed_header.value->width;
  report.height = parsed_header.value->height;
  report.payload_size = bytes.size() - report.header_size;
  report.payload_expected_from_dimensions = static_cast<std::size_t>(report.width) * static_cast<std::size_t>(report.height);
  report.payload_surplus_or_deficit =
      static_cast<std::ptrdiff_t>(report.payload_size) - static_cast<std::ptrdiff_t>(report.payload_expected_from_dimensions);

  constexpr std::size_t kPreviewBytes = 8;
  const auto payload_preview_count = std::min(kPreviewBytes, report.payload_size);
  copy_preview(bytes, report.payload_offset, payload_preview_count, report.payload_prefix_preview);
  if (payload_preview_count > 0) {
    copy_preview(bytes,
                 report.payload_offset + (report.payload_size - payload_preview_count),
                 payload_preview_count,
                 report.payload_suffix_preview);
  }

  if (report.height > 0 && report.payload_size % report.height == 0) {
    report.derived_row_stride = report.payload_size / report.height;
  }

  if (report.derived_row_stride >= report.width && report.height > 0) {
    report.has_row_padding_hint = true;
    report.row_padding_hint = report.derived_row_stride - report.width;
  }

  if (report.payload_size > report.payload_expected_from_dimensions) {
    const auto extra_offset = report.payload_offset + report.payload_expected_from_dimensions;
    const auto extra_size = report.payload_size - report.payload_expected_from_dimensions;
    const auto extra_preview_count = std::min(kPreviewBytes, extra_size);
    copy_preview(bytes, extra_offset, extra_preview_count, report.extra_prefix_preview);
    copy_preview(bytes, extra_offset + (extra_size - extra_preview_count), extra_preview_count, report.extra_suffix_preview);

    if (report.height > 0 && extra_size % report.height == 0) {
      report.has_trailing_block_hint = true;
      report.trailing_block_bytes_hint = extra_size / report.height;
    }
  }

  return {.value = std::move(report)};
}

ParseResult<Pl8ImageVariantProbeReport> probe_caesar2_large_pl8_image_variant(std::span<const std::uint8_t> bytes) {
  return probe_caesar2_large_pl8_image_variant(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
}

ParseResult<StructuredPl8StructuredRegionsProbeReport> probe_caesar2_rat_back_structured_pl8_regions(
    std::span<const std::byte> bytes) {
  const auto parsed_header = parse_large_pl8_common_header(bytes);
  if (!parsed_header.ok()) {
    if (parsed_header.error->message.find("Unsupported large-PL8 image layout") != std::string::npos) {
      std::ostringstream message;
      message << "Unsupported RAT_BACK-style structured PL8 reconnaissance layout: expected at least header_size="
              << Pl8ImageResource::kHeaderSize << " bytes, got " << bytes.size();
      return {.error = make_invalid_pl8_image_error(Pl8ImageResource::kHeaderSize, bytes.size(), message.str())};
    }
    return {.error = parsed_header.error};
  }

  StructuredPl8StructuredRegionsProbeReport report;
  report.file_size = bytes.size();
  report.header_size = Pl8ImageResource::kHeaderSize;
  report.payload_offset = Pl8ImageResource::kHeaderSize;
  report.width = parsed_header.value->width;
  report.height = parsed_header.value->height;
  report.payload_size = bytes.size() - report.header_size;
  report.expected_image_size = static_cast<std::size_t>(report.width) * static_cast<std::size_t>(report.height);
  report.payload_surplus_or_deficit =
      static_cast<std::ptrdiff_t>(report.payload_size) - static_cast<std::ptrdiff_t>(report.expected_image_size);

  if (report.payload_size < 8) {
    std::ostringstream message;
    message << "RAT_BACK-style structured reconnaissance payload too small: requires at least 8 bytes after header, got "
            << report.payload_size;
    return {.error = make_invalid_pl8_image_error(8, report.payload_size, message.str())};
  }

  const auto* payload_begin = bytes.data() + report.payload_offset;
  constexpr std::size_t kMaxLeadingFields = 8;
  const auto leading_field_count = std::min(kMaxLeadingFields, report.payload_size / sizeof(std::uint32_t));
  report.leading_fields.reserve(leading_field_count);
  for (std::size_t field_index = 0; field_index < leading_field_count; ++field_index) {
    const auto byte_offset = field_index * sizeof(std::uint32_t);
    const auto* field_ptr = payload_begin + byte_offset;
    const auto low_u16 = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(field_ptr[0])) |
        (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(field_ptr[1])) << 8U));
    const auto u32 =
        static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(field_ptr[0])) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(field_ptr[1])) << 8U) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(field_ptr[2])) << 16U) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(field_ptr[3])) << 24U);
    report.leading_fields.push_back({.index = field_index, .value_u16le = low_u16, .value_u32le = u32});
  }

  constexpr std::array<std::size_t, 5> kRecordSizes = {4, 6, 8, 12, 16};
  constexpr std::size_t kMaxScannedRecords = 64;
  for (const auto record_size : kRecordSizes) {
    if (report.payload_size < record_size * 2) {
      continue;
    }

    const auto scanned_records = std::min(kMaxScannedRecords, report.payload_size / record_size);
    std::size_t plausible_pairs = 0;
    std::size_t monotonic_offsets = 0;
    std::size_t repeated_records = 0;
    std::optional<std::size_t> previous_start;
    std::map<std::string, std::size_t> record_histogram;

    for (std::size_t record_index = 0; record_index < scanned_records; ++record_index) {
      const auto record_base = payload_begin + (record_index * record_size);
      std::ostringstream signature_builder;
      for (std::size_t byte_index = 0; byte_index < record_size; ++byte_index) {
        signature_builder << static_cast<unsigned int>(std::to_integer<std::uint8_t>(record_base[byte_index])) << ",";
      }
      const auto inserted = record_histogram.insert_or_assign(signature_builder.str(), 0);
      if (!inserted.second) {
        inserted.first->second += 1;
        repeated_records += 1;
      } else {
        inserted.first->second = 1;
      }

      std::size_t start = 0;
      std::size_t length = 0;
      if (record_size >= 8) {
        start = static_cast<std::size_t>(static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(record_base[0])) |
                                         (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(record_base[1])) << 8U) |
                                         (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(record_base[2])) << 16U) |
                                         (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(record_base[3])) << 24U));
        length = static_cast<std::size_t>(static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(record_base[4])) |
                                          (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(record_base[5])) << 8U) |
                                          (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(record_base[6])) << 16U) |
                                          (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(record_base[7])) << 24U));
      } else {
        start = static_cast<std::size_t>(static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(record_base[0])) |
                                         (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(record_base[1])) << 8U));
        length = static_cast<std::size_t>(static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(record_base[2])) |
                                          (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(record_base[3])) << 8U));
      }

      if (length > 0 && start < report.payload_size && start + length <= report.payload_size) {
        plausible_pairs += 1;
        if (previous_start.has_value() && start >= previous_start.value()) {
          monotonic_offsets += 1;
        }
        previous_start = start;
      }
    }

    report.record_hints.push_back({.record_size = record_size,
                                   .scanned_records = scanned_records,
                                   .plausible_offset_length_pairs = plausible_pairs,
                                   .monotonic_offset_pairs = monotonic_offsets,
                                   .repeated_record_count = repeated_records});
  }

  constexpr std::size_t kMaxPairRegions = 12;
  for (std::size_t field_index = 0; field_index + 1 < report.leading_fields.size() &&
                                   report.region_hints.size() < kMaxPairRegions;
       field_index += 2) {
    const auto start = static_cast<std::size_t>(report.leading_fields[field_index].value_u32le);
    const auto size = static_cast<std::size_t>(report.leading_fields[field_index + 1].value_u32le);
    const bool in_bounds = size > 0 && start < report.payload_size && start + size <= report.payload_size;
    report.region_hints.push_back(
        {.source = "leading_u32_pairs", .entry_index = field_index / 2, .start_offset = start, .region_size = size, .in_bounds = in_bounds});
  }

  constexpr std::size_t kMaxOffsetTableFields = 16;
  std::vector<std::size_t> offsets;
  offsets.reserve(kMaxOffsetTableFields);
  for (std::size_t index = 0; index < std::min(kMaxOffsetTableFields, report.leading_fields.size()); ++index) {
    const auto value = static_cast<std::size_t>(report.leading_fields[index].value_u32le);
    if (value > 0 && value < report.payload_size) {
      offsets.push_back(value);
    }
  }
  std::sort(offsets.begin(), offsets.end());
  offsets.erase(std::unique(offsets.begin(), offsets.end()), offsets.end());
  if (offsets.size() >= 2) {
    for (std::size_t index = 0; index + 1 < offsets.size() && report.region_hints.size() < kMaxPairRegions; ++index) {
      const auto start = offsets[index];
      const auto next = offsets[index + 1];
      if (next <= start) {
        continue;
      }
      report.region_hints.push_back({.source = "monotonic_offset_table",
                                     .entry_index = index,
                                     .start_offset = start,
                                     .region_size = next - start,
                                     .in_bounds = true});
    }
  }

  return {.value = std::move(report)};
}

ParseResult<StructuredPl8StructuredRegionsProbeReport> probe_caesar2_rat_back_structured_pl8_regions(
    std::span<const std::uint8_t> bytes) {
  return probe_caesar2_rat_back_structured_pl8_regions(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
}

ParseResult<Pl8Image256PairDecodeResult> decode_caesar2_forum_pl8_image_pair(
    std::span<const std::uint8_t> image_pl8_bytes,
    std::span<const std::uint8_t> palette_256_bytes,
    const bool index_zero_transparent) {
  const auto parsed_image = parse_caesar2_forum_pl8_image(image_pl8_bytes);
  if (!parsed_image.ok()) {
    return {.error = parsed_image.error};
  }

  const auto decoded_indexed = decode_indexed_with_256_palette(parsed_image.value->indexed_pixels,
                                                               parsed_image.value->width,
                                                               parsed_image.value->height,
                                                               palette_256_bytes,
                                                               index_zero_transparent);
  if (!decoded_indexed.ok()) {
    return {.error = decoded_indexed.error};
  }

  Pl8Image256PairDecodeResult decoded;
  decoded.image_pl8 = parsed_image.value.value();
  decoded.palette_256 = decoded_indexed.value->palette_256;
  decoded.rgba_image = decoded_indexed.value->rgba_image;
  return {.value = std::move(decoded)};
}

ParseResult<StructuredPl8Image256PairDecodeResult> decode_caesar2_rat_back_structured_pl8_image_pair(
    std::span<const std::uint8_t> image_pl8_bytes,
    std::span<const std::uint8_t> palette_256_bytes,
    const bool index_zero_transparent) {
  const auto parsed_image = parse_caesar2_rat_back_structured_pl8_image(image_pl8_bytes);
  if (!parsed_image.ok()) {
    return {.error = parsed_image.error};
  }

  const auto decoded_indexed = decode_indexed_with_256_palette(parsed_image.value->indexed_pixels,
                                                               parsed_image.value->width,
                                                               parsed_image.value->height,
                                                               palette_256_bytes,
                                                               index_zero_transparent);
  if (!decoded_indexed.ok()) {
    return {.error = decoded_indexed.error};
  }

  StructuredPl8Image256PairDecodeResult decoded;
  decoded.image_pl8 = parsed_image.value.value();
  decoded.palette_256 = decoded_indexed.value->palette_256;
  decoded.rgba_image = decoded_indexed.value->rgba_image;
  return {.value = std::move(decoded)};
}

std::string format_pl8_image_report(const Pl8ImageResource& image, const std::size_t max_pixels) {
  std::ostringstream output;
  output << "# Caesar II Win95 FORUM-style PL8 Image Report\n";
  output << "supported_layout: forum_pl8_image_24b_header\n";
  output << "header_size: " << image.header_size << "\n";
  output << "payload_offset: " << image.payload_offset << "\n";
  output << "width: " << image.width << "\n";
  output << "height: " << image.height << "\n";
  output << "payload_size: " << image.payload_size << "\n";
  output << "payload_expected_from_dimensions: " << (static_cast<std::size_t>(image.width) * image.height) << "\n";
  output << "payload_matches_dimensions: "
         << (image.payload_size == (static_cast<std::size_t>(image.width) * image.height) ? "yes" : "no") << "\n";

  const auto count = std::min(max_pixels, image.indexed_pixels.size());
  output << "pixels:";
  for (std::size_t index = 0; index < count; ++index) {
    output << (index == 0 ? " " : ",") << static_cast<unsigned int>(image.indexed_pixels[index]);
  }
  output << "\n";

  if (count < image.indexed_pixels.size()) {
    output << "... (" << (image.indexed_pixels.size() - count) << " more pixels)\n";
  }

  return output.str();
}

std::string format_pl8_structured_report(const StructuredPl8Resource& image) {
  std::ostringstream output;
  output << "# Caesar II Win95 RAT_BACK-style Structured PL8 Report\n";
  output << "supported_layout: rat_back_structured_pl8_24b_header\n";
  output << "header_size: " << image.header_size << "\n";
  output << "payload_offset: " << image.payload_offset << "\n";
  output << "width: " << image.width << "\n";
  output << "height: " << image.height << "\n";
  output << "payload_size: " << image.payload_size << "\n";
  output << "payload_expected_from_dimensions: " << image.expected_image_size << "\n";
  output << "prefix_field_count: " << image.prefix_fields.size() << "\n";
  output << "prefix_fields:\n";
  for (const auto& field : image.prefix_fields) {
    output << "  - field_" << field.index << "_u32le: " << field.value << "\n";
  }

  output << "candidate_regions:\n";
  for (const auto& candidate : image.candidate_regions) {
    output << "  - offset_field: field_" << candidate.offset_field_index << "\n";
    output << "    size_field: field_" << candidate.size_field_index << "\n";
    output << "    payload_offset: " << candidate.payload_offset << "\n";
    output << "    payload_size: " << candidate.payload_size << "\n";
    output << "    accepted: " << (candidate.accepted ? "yes" : "no") << "\n";
    output << "    reason: " << candidate.reason << "\n";
  }

  output << "primary_image_region_identified: " << (image.selected_region.has_value() ? "yes" : "no") << "\n";
  if (image.selected_region.has_value()) {
    output << "selected_region_offset: " << image.selected_region->payload_offset << "\n";
    output << "selected_region_size: " << image.selected_region->payload_size << "\n";
    output << "decode_status: " << (image.indexed_pixels.size() == image.expected_image_size ? "decoded" : "failed") << "\n";
  } else {
    output << "decode_status: failed\n";
  }
  return output.str();
}

std::string format_pl8_image_variant_probe_report(const Pl8ImageVariantProbeReport& report,
                                                  const std::size_t preview_bytes) {
  std::ostringstream output;
  output << "# Caesar II Win95 Large-PL8 Variant Probe Report\n";
  output << "header_size: " << report.header_size << "\n";
  output << "payload_offset: " << report.payload_offset << "\n";
  output << "file_size: " << report.file_size << "\n";
  output << "width: " << report.width << "\n";
  output << "height: " << report.height << "\n";
  output << "payload_bytes_after_header: " << report.payload_size << "\n";
  output << "payload_expected_from_dimensions: " << report.payload_expected_from_dimensions << "\n";
  output << "payload_surplus_or_deficit: " << report.payload_surplus_or_deficit << "\n";
  output << "payload_matches_forum_layout: " << (report.payload_surplus_or_deficit == 0 ? "yes" : "no") << "\n";

  if (report.derived_row_stride > 0) {
    output << "row_stride_hint: " << report.derived_row_stride << "\n";
  } else {
    output << "row_stride_hint: unavailable\n";
  }

  if (report.has_row_padding_hint) {
    output << "row_padding_hint: " << report.row_padding_hint << "\n";
  } else {
    output << "row_padding_hint: unavailable\n";
  }

  if (report.has_trailing_block_hint) {
    output << "trailing_block_per_row_hint: " << report.trailing_block_bytes_hint << "\n";
  } else {
    output << "trailing_block_per_row_hint: unavailable\n";
  }

  const auto bounded_payload_prefix =
      std::span<const std::uint8_t>(report.payload_prefix_preview.data(),
                                    std::min(preview_bytes, report.payload_prefix_preview.size()));
  const auto bounded_payload_suffix =
      std::span<const std::uint8_t>(report.payload_suffix_preview.data(),
                                    std::min(preview_bytes, report.payload_suffix_preview.size()));
  const auto bounded_extra_prefix =
      std::span<const std::uint8_t>(report.extra_prefix_preview.data(),
                                    std::min(preview_bytes, report.extra_prefix_preview.size()));
  const auto bounded_extra_suffix =
      std::span<const std::uint8_t>(report.extra_suffix_preview.data(),
                                    std::min(preview_bytes, report.extra_suffix_preview.size()));

  output << "payload_prefix_preview: " << format_preview_bytes(bounded_payload_prefix) << "\n";
  output << "payload_suffix_preview: " << format_preview_bytes(bounded_payload_suffix) << "\n";
  output << "extra_payload_prefix_preview: " << format_preview_bytes(bounded_extra_prefix) << "\n";
  output << "extra_payload_suffix_preview: " << format_preview_bytes(bounded_extra_suffix) << "\n";
  return output.str();
}

std::string format_pl8_image_variant_comparison_report(const Pl8ImageVariantProbeReport& lhs,
                                                       const std::string& lhs_label,
                                                       const Pl8ImageVariantProbeReport& rhs,
                                                       const std::string& rhs_label,
                                                       const std::size_t preview_bytes) {
  std::ostringstream output;
  output << "# Caesar II Win95 Large-PL8 Variant Comparison\n";
  output << "lhs_label: " << lhs_label << "\n";
  output << "rhs_label: " << rhs_label << "\n";
  output << "width_equal: " << (lhs.width == rhs.width ? "yes" : "no") << "\n";
  output << "height_equal: " << (lhs.height == rhs.height ? "yes" : "no") << "\n";
  output << "payload_expected_equal: "
         << (lhs.payload_expected_from_dimensions == rhs.payload_expected_from_dimensions ? "yes" : "no") << "\n";
  output << "payload_size_delta_rhs_minus_lhs: "
         << (static_cast<std::ptrdiff_t>(rhs.payload_size) - static_cast<std::ptrdiff_t>(lhs.payload_size)) << "\n";
  output << "payload_surplus_delta_rhs_minus_lhs: "
         << (rhs.payload_surplus_or_deficit - lhs.payload_surplus_or_deficit) << "\n";
  output << "\n";
  output << "## lhs\n";
  output << format_pl8_image_variant_probe_report(lhs, preview_bytes);
  output << "## rhs\n";
  output << format_pl8_image_variant_probe_report(rhs, preview_bytes);
  return output.str();
}

std::string format_pl8_structured_regions_probe_report(const StructuredPl8StructuredRegionsProbeReport& report) {
  std::ostringstream output;
  output << "# Caesar II Win95 RAT_BACK-style Structured PL8 Regions Reconnaissance\n";
  output << "supported_layout: rat_back_structured_regions_probe_24b_header\n";
  output << "header_size: " << report.header_size << "\n";
  output << "payload_offset: " << report.payload_offset << "\n";
  output << "file_size: " << report.file_size << "\n";
  output << "width: " << report.width << "\n";
  output << "height: " << report.height << "\n";
  output << "payload_bytes_after_header: " << report.payload_size << "\n";
  output << "payload_expected_from_dimensions: " << report.expected_image_size << "\n";
  output << "payload_surplus_or_deficit: " << report.payload_surplus_or_deficit << "\n";
  output << "leading_fields:\n";
  for (const auto& field : report.leading_fields) {
    output << "  - field_" << field.index << "_u16le: " << field.value_u16le << "\n";
    output << "    field_" << field.index << "_u32le: " << field.value_u32le << "\n";
  }
  output << "record_hints:\n";
  for (const auto& hint : report.record_hints) {
    output << "  - record_size: " << hint.record_size << "\n";
    output << "    scanned_records: " << hint.scanned_records << "\n";
    output << "    plausible_offset_length_pairs: " << hint.plausible_offset_length_pairs << "\n";
    output << "    monotonic_offset_pairs: " << hint.monotonic_offset_pairs << "\n";
    output << "    repeated_record_count: " << hint.repeated_record_count << "\n";
  }
  output << "candidate_regions:\n";
  for (const auto& region : report.region_hints) {
    output << "  - source: " << region.source << "\n";
    output << "    entry_index: " << region.entry_index << "\n";
    output << "    start_offset: " << region.start_offset << "\n";
    output << "    region_size: " << region.region_size << "\n";
    output << "    in_bounds: " << (region.in_bounds ? "yes" : "no") << "\n";
  }
  output << "decode_status: not_attempted\n";
  return output.str();
}

std::string format_pl8_structured_regions_comparison_report(
    const StructuredPl8StructuredRegionsProbeReport& lhs,
    const std::string& lhs_label,
    const StructuredPl8StructuredRegionsProbeReport& rhs,
    const std::string& rhs_label) {
  std::ostringstream output;
  output << "# Caesar II Win95 RAT_BACK-style Structured PL8 Regions Comparison\n";
  output << "lhs_label: " << lhs_label << "\n";
  output << "rhs_label: " << rhs_label << "\n";
  output << "width_equal: " << (lhs.width == rhs.width ? "yes" : "no") << "\n";
  output << "height_equal: " << (lhs.height == rhs.height ? "yes" : "no") << "\n";
  output << "payload_size_delta_rhs_minus_lhs: "
         << (static_cast<std::ptrdiff_t>(rhs.payload_size) - static_cast<std::ptrdiff_t>(lhs.payload_size)) << "\n";
  output << "payload_surplus_delta_rhs_minus_lhs: "
         << (rhs.payload_surplus_or_deficit - lhs.payload_surplus_or_deficit) << "\n";
  output << "record_hint_count_lhs: " << lhs.record_hints.size() << "\n";
  output << "record_hint_count_rhs: " << rhs.record_hints.size() << "\n";
  output << "region_hint_count_lhs: " << lhs.region_hints.size() << "\n";
  output << "region_hint_count_rhs: " << rhs.region_hints.size() << "\n";
  output << "\n## lhs\n";
  output << format_pl8_structured_regions_probe_report(lhs);
  output << "## rhs\n";
  output << format_pl8_structured_regions_probe_report(rhs);
  return output.str();
}

}  // namespace romulus::data
