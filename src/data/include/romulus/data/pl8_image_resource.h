#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
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
  static constexpr std::size_t kDimensionsOffset = 8;
  static constexpr std::uint16_t kMinSupportedDimension = 1;
  static constexpr std::uint16_t kMaxSupportedDimension = 4096;

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

struct StructuredPl8PrefixField {
  std::size_t index = 0;
  std::uint32_t value = 0;
};

struct StructuredPl8CandidateRegion {
  std::size_t offset_field_index = 0;
  std::size_t size_field_index = 0;
  std::size_t payload_offset = 0;
  std::size_t payload_size = 0;
  bool accepted = false;
  std::string reason;
};

struct StructuredPl8Resource {
  static constexpr std::size_t kStructuredPrefixSize = 16;

  std::size_t header_size = 0;
  std::size_t payload_offset = 0;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::size_t payload_size = 0;
  std::size_t expected_image_size = 0;
  std::vector<StructuredPl8PrefixField> prefix_fields;
  std::vector<StructuredPl8CandidateRegion> candidate_regions;
  std::optional<StructuredPl8CandidateRegion> selected_region;
  std::vector<std::uint8_t> indexed_pixels;
};

struct StructuredPl8Image256PairDecodeResult {
  StructuredPl8Resource image_pl8;
  Pl8Resource palette_256;
  RgbaImage rgba_image;
};

struct StructuredPl8LeadingFieldSample {
  std::size_t index = 0;
  std::uint16_t value_u16le = 0;
  std::uint32_t value_u32le = 0;
};

struct StructuredPl8RecordHint {
  std::size_t record_size = 0;
  std::size_t scanned_records = 0;
  std::size_t plausible_offset_length_pairs = 0;
  std::size_t monotonic_offset_pairs = 0;
  std::size_t repeated_record_count = 0;
  std::size_t deterministic_record_count = 0;
};

struct StructuredPl8OffsetLengthPairHint {
  std::string source;
  std::size_t entry_index = 0;
  std::size_t start_offset = 0;
  std::size_t length = 0;
  bool in_bounds = false;
};

struct StructuredPl8RegionHint {
  std::string source;
  std::size_t entry_index = 0;
  std::size_t start_offset = 0;
  std::size_t region_size = 0;
  bool in_bounds = false;
  std::string size_classification;
  std::vector<std::uint8_t> prefix_preview;
};

struct StructuredPl8StructuredRegionsProbeReport {
  std::size_t file_size = 0;
  std::size_t header_size = 0;
  std::size_t payload_offset = 0;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::size_t payload_size = 0;
  std::size_t expected_image_size = 0;
  std::ptrdiff_t payload_surplus_or_deficit = 0;
  std::vector<StructuredPl8LeadingFieldSample> leading_fields;
  std::vector<StructuredPl8RecordHint> record_hints;
  std::vector<StructuredPl8OffsetLengthPairHint> offset_length_pair_hints;
  std::vector<StructuredPl8RegionHint> region_hints;
};

struct Pl8ImageVariantProbeReport {
  std::size_t file_size = 0;
  std::size_t header_size = 0;
  std::size_t payload_offset = 0;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::size_t payload_size = 0;
  std::size_t payload_expected_from_dimensions = 0;
  std::ptrdiff_t payload_surplus_or_deficit = 0;
  std::size_t derived_row_stride = 0;
  bool has_row_padding_hint = false;
  std::size_t row_padding_hint = 0;
  bool has_trailing_block_hint = false;
  std::size_t trailing_block_bytes_hint = 0;
  std::vector<std::uint8_t> payload_prefix_preview;
  std::vector<std::uint8_t> payload_suffix_preview;
  std::vector<std::uint8_t> extra_prefix_preview;
  std::vector<std::uint8_t> extra_suffix_preview;
};

[[nodiscard]] ParseResult<Pl8ImageResource> parse_caesar2_forum_pl8_image(std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<Pl8ImageResource> parse_caesar2_forum_pl8_image(std::span<const std::uint8_t> bytes);
[[nodiscard]] ParseResult<StructuredPl8Resource> parse_caesar2_rat_back_structured_pl8_image(
    std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<StructuredPl8Resource> parse_caesar2_rat_back_structured_pl8_image(
    std::span<const std::uint8_t> bytes);
[[nodiscard]] ParseResult<Pl8ImageVariantProbeReport> probe_caesar2_large_pl8_image_variant(
    std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<Pl8ImageVariantProbeReport> probe_caesar2_large_pl8_image_variant(
    std::span<const std::uint8_t> bytes);
[[nodiscard]] ParseResult<StructuredPl8StructuredRegionsProbeReport> probe_caesar2_rat_back_structured_pl8_regions(
    std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<StructuredPl8StructuredRegionsProbeReport> probe_caesar2_rat_back_structured_pl8_regions(
    std::span<const std::uint8_t> bytes);

[[nodiscard]] ParseResult<Pl8Image256PairDecodeResult> decode_caesar2_forum_pl8_image_pair(
    std::span<const std::uint8_t> image_pl8_bytes,
    std::span<const std::uint8_t> palette_256_bytes,
    bool index_zero_transparent = false);
[[nodiscard]] ParseResult<StructuredPl8Image256PairDecodeResult> decode_caesar2_rat_back_structured_pl8_image_pair(
    std::span<const std::uint8_t> image_pl8_bytes,
    std::span<const std::uint8_t> palette_256_bytes,
    bool index_zero_transparent = false);

[[nodiscard]] std::string format_pl8_image_report(const Pl8ImageResource& image, std::size_t max_pixels = 32);
[[nodiscard]] std::string format_pl8_structured_report(const StructuredPl8Resource& image);
[[nodiscard]] std::string format_pl8_image_variant_probe_report(const Pl8ImageVariantProbeReport& report,
                                                                std::size_t preview_bytes = 8);
[[nodiscard]] std::string format_pl8_image_variant_comparison_report(const Pl8ImageVariantProbeReport& lhs,
                                                                     const std::string& lhs_label,
                                                                     const Pl8ImageVariantProbeReport& rhs,
                                                                     const std::string& rhs_label,
                                                                     std::size_t preview_bytes = 8);
[[nodiscard]] std::string format_pl8_structured_regions_probe_report(
    const StructuredPl8StructuredRegionsProbeReport& report);
[[nodiscard]] std::string format_pl8_structured_regions_comparison_report(
    const StructuredPl8StructuredRegionsProbeReport& lhs,
    const std::string& lhs_label,
    const StructuredPl8StructuredRegionsProbeReport& rhs,
    const std::string& rhs_label);

}  // namespace romulus::data
