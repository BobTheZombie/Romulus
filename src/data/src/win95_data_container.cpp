#include "romulus/data/win95_data_container.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <utility>

#include "romulus/data/file_loader.h"

namespace romulus::data {
namespace {

[[nodiscard]] ParseError make_invalid_container_error(std::size_t offset,
                                                      std::size_t requested_bytes,
                                                      std::size_t buffer_size,
                                                      const std::string& message) {
  return make_invalid_format_error(offset, requested_bytes, buffer_size, message);
}

[[nodiscard]] ParseError make_out_of_bounds_container_error(std::size_t offset,
                                                            std::size_t requested_bytes,
                                                            std::size_t buffer_size,
                                                            const std::string& message) {
  auto error = make_out_of_bounds_error(offset, requested_bytes, buffer_size);
  error.message = message;
  return error;
}

[[nodiscard]] std::optional<std::size_t> checked_add(std::size_t left, std::size_t right) {
  if (right > std::numeric_limits<std::size_t>::max() - left) {
    return std::nullopt;
  }

  return left + right;
}

[[nodiscard]] std::string compact_failure_reason(std::string_view message) {
  constexpr std::string_view kPrefix = "failed ILBM validation: ";
  constexpr std::string_view kTextPrefix = "failed text validation: ";
  constexpr std::string_view kProbePrefix = "PACK entry ";
  const auto validation_index = message.find(kPrefix);
  if (validation_index != std::string_view::npos) {
    return std::string(message.substr(validation_index + kPrefix.size()));
  }
  const auto text_validation_index = message.find(kTextPrefix);
  if (text_validation_index != std::string_view::npos) {
    return std::string(message.substr(text_validation_index + kTextPrefix.size()));
  }

  const auto colon_index = message.find(": ");
  if (colon_index != std::string_view::npos && message.substr(0, kProbePrefix.size()) == kProbePrefix) {
    return std::string(message.substr(colon_index + 2));
  }

  return std::string(message);
}

[[nodiscard]] bool is_supported_text_character(const unsigned char value) {
  return (value >= 0x20 && value <= 0x7E) || value == '\n' || value == '\r' || value == '\t';
}

[[nodiscard]] bool is_supported_pl8_like_candidate(const Win95PackContainerEntry& entry) {
  if (entry.size != Pl8Resource::kSupportedPayloadSize) {
    return false;
  }

  if (entry.classification_hint == "possible-ilbm" || entry.classification_hint == "text-like" ||
      entry.classification_hint == "empty") {
    return false;
  }

  return true;
}

[[nodiscard]] std::size_t count_text_lines(std::string_view decoded_text) {
  if (decoded_text.empty()) {
    return 0;
  }

  return 1 + static_cast<std::size_t>(std::count(decoded_text.begin(), decoded_text.end(), '\n'));
}

[[nodiscard]] std::string make_text_preview(std::string_view text, std::size_t limit, bool* truncated_out) {
  const auto preview_size = std::min(limit, text.size());
  std::string preview;
  preview.reserve(preview_size);

  for (std::size_t index = 0; index < preview_size; ++index) {
    const auto value = static_cast<unsigned char>(text[index]);
    if (value == '\n') {
      preview += "\\n";
    } else if (value == '\r') {
      preview += "\\r";
    } else if (value == '\t') {
      preview += "\\t";
    } else {
      preview.push_back(static_cast<char>(value));
    }
  }

  const bool truncated = preview_size < text.size();
  if (truncated_out != nullptr) {
    *truncated_out = truncated;
  }

  return preview;
}

[[nodiscard]] std::string_view to_family_label(const Win95PackKnownFamilyKind family_kind) {
  switch (family_kind) {
    case Win95PackKnownFamilyKind::Ilbm:
      return "ilbm";
    case Win95PackKnownFamilyKind::Text:
      return "text";
    case Win95PackKnownFamilyKind::Pl8:
      return "pl8";
  }

  return "unknown";
}

struct RangeCheck {
  std::size_t index = 0;
  std::size_t offset = 0;
  std::size_t end_offset = 0;
};

constexpr std::size_t k_entry_signature_preview_bytes = 12;
constexpr std::size_t k_summary_magic_limit = 6;

[[nodiscard]] std::string to_hex_signature(std::span<const std::byte> bytes) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    if (index != 0) {
      stream << ' ';
    }

    stream << std::setw(2) << static_cast<unsigned int>(std::to_integer<std::uint8_t>(bytes[index]));
  }

  return stream.str();
}

[[nodiscard]] std::string to_ascii_signature(std::span<const std::byte> bytes) {
  std::string output;
  output.reserve(bytes.size());
  for (const auto byte : bytes) {
    const auto character = std::to_integer<unsigned char>(byte);
    output.push_back(std::isprint(character) != 0 ? static_cast<char>(character) : '.');
  }

  return output;
}

[[nodiscard]] bool is_text_like(std::span<const std::byte> bytes) {
  if (bytes.empty()) {
    return false;
  }

  std::size_t printable_count = 0;
  for (const auto byte : bytes) {
    const auto value = std::to_integer<unsigned char>(byte);
    if (value == 0) {
      return false;
    }

    if (std::isprint(value) != 0 || value == '\n' || value == '\r' || value == '\t') {
      ++printable_count;
    }
  }

  return printable_count * 100 >= bytes.size() * 90;
}

[[nodiscard]] std::string classify_entry_hint(std::span<const std::byte> bytes) {
  if (bytes.size() >= 4 && bytes[0] == std::byte{'F'} && bytes[1] == std::byte{'O'} && bytes[2] == std::byte{'R'} &&
      bytes[3] == std::byte{'M'}) {
    return "possible-ilbm";
  }

  if (is_text_like(bytes)) {
    return "text-like";
  }

  if (bytes.empty()) {
    return "empty";
  }

  return "opaque-binary";
}

[[nodiscard]] bool has_recognizable_signature(std::span<const std::byte> bytes, std::string_view classification_hint) {
  if (classification_hint == "possible-ilbm" || classification_hint == "text-like") {
    return true;
  }

  if (bytes.size() >= 4 &&
      ((bytes[0] == std::byte{'R'} && bytes[1] == std::byte{'I'} && bytes[2] == std::byte{'F'} && bytes[3] == std::byte{'F'}) ||
       (bytes[0] == std::byte{'M'} && bytes[1] == std::byte{'Z'}))) {
    return true;
  }

  return false;
}

[[nodiscard]] std::string make_magic_key(std::span<const std::byte> bytes) {
  if (bytes.empty()) {
    return "(empty)";
  }

  const auto preview_size = std::min<std::size_t>(4, bytes.size());
  return to_hex_signature(bytes.first(preview_size));
}

void update_size_bucket(Win95PackEntrySizeBuckets& buckets, const std::size_t size) {
  if (size <= 255) {
    ++buckets.tiny_bytes_0_to_255;
    return;
  }

  if (size <= 4095) {
    ++buckets.small_bytes_256_to_4095;
    return;
  }

  if (size <= 65535) {
    ++buckets.medium_bytes_4096_to_65535;
    return;
  }

  ++buckets.large_bytes_65536_plus;
}

}  // namespace

ParseResult<Win95PackContainerResource> parse_win95_pack_container(std::span<const std::byte> bytes) {
  BinaryReader reader(bytes);

  const auto magic = reader.read_bytes(4);
  if (!magic.ok()) {
    return {.error = magic.error};
  }

  constexpr char kExpectedMagic[] = "PACK";
  const auto magic_span = magic.value.value();
  if (magic_span[0] != std::byte{'P'} || magic_span[1] != std::byte{'A'} || magic_span[2] != std::byte{'C'} ||
      magic_span[3] != std::byte{'K'}) {
    return {.error = make_invalid_container_error(
                0,
                4,
                bytes.size(),
                "Unsupported DATA/DATA0 layout: expected PACK header signature")};
  }

  const auto entry_count = reader.read_u32_le();
  if (!entry_count.ok()) {
    return {.error = entry_count.error};
  }

  const auto entry_count_wide = static_cast<std::uint64_t>(entry_count.value.value());
  const auto max_size = static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max());
  const auto table_size_wide = entry_count_wide * static_cast<std::uint64_t>(Win95PackContainerHeader::kEntrySize);
  if (table_size_wide > max_size) {
    return {.error = make_invalid_container_error(
                4,
                4,
                bytes.size(),
                "Entry table size exceeds host size limits")};
  }

  const auto table_size = static_cast<std::size_t>(table_size_wide);
  const auto table_end = checked_add(Win95PackContainerHeader::kHeaderSize, table_size);
  if (!table_end.has_value()) {
    return {.error = make_invalid_container_error(
                4,
                4,
                bytes.size(),
                "Entry table end offset overflows host size")};
  }

  if (table_end.value() > bytes.size()) {
    std::ostringstream message;
    message << "Entry table exceeds file bounds: table_end=" << table_end.value() << " file_size=" << bytes.size();
    return {.error = make_invalid_container_error(4, 4, bytes.size(), message.str())};
  }

  Win95PackContainerResource container;
  container.header.signature = kExpectedMagic;
  container.header.entry_count = entry_count.value.value();
  container.header.entry_table_offset = Win95PackContainerHeader::kHeaderSize;
  container.header.entry_table_size = table_size;
  container.entries.reserve(static_cast<std::size_t>(entry_count.value.value()));
  container.summary.entry_count = static_cast<std::size_t>(entry_count.value.value());

  std::vector<RangeCheck> ranges;
  ranges.reserve(static_cast<std::size_t>(entry_count.value.value()));
  std::map<std::string, std::size_t> magic_frequency_map;

  for (std::size_t index = 0; index < static_cast<std::size_t>(entry_count.value.value()); ++index) {
    const auto entry_offset = reader.read_u32_le();
    if (!entry_offset.ok()) {
      return {.error = entry_offset.error};
    }

    const auto entry_size = reader.read_u32_le();
    if (!entry_size.ok()) {
      return {.error = entry_size.error};
    }

    const auto offset = static_cast<std::size_t>(entry_offset.value.value());
    const auto size = static_cast<std::size_t>(entry_size.value.value());

    if (offset < table_end.value()) {
      std::ostringstream message;
      message << "Entry " << index << " points into container header/table region";
      const auto entry_record_offset = Win95PackContainerHeader::kHeaderSize + (index * Win95PackContainerHeader::kEntrySize);
      return {.error = make_invalid_container_error(entry_record_offset, Win95PackContainerHeader::kEntrySize, bytes.size(), message.str())};
    }

    const auto end_offset = checked_add(offset, size);
    if (!end_offset.has_value()) {
      std::ostringstream message;
      message << "Entry " << index << " range overflows host size";
      const auto entry_record_offset = Win95PackContainerHeader::kHeaderSize + (index * Win95PackContainerHeader::kEntrySize);
      return {.error = make_invalid_container_error(entry_record_offset, Win95PackContainerHeader::kEntrySize, bytes.size(), message.str())};
    }

    if (end_offset.value() > bytes.size()) {
      std::ostringstream message;
      message << "Entry " << index << " range exceeds file bounds";
      const auto entry_record_offset = Win95PackContainerHeader::kHeaderSize + (index * Win95PackContainerHeader::kEntrySize);
      return {.error = make_invalid_container_error(entry_record_offset, Win95PackContainerHeader::kEntrySize, bytes.size(), message.str())};
    }

    const auto entry_bytes = bytes.subspan(offset, size);
    const auto signature_bytes = entry_bytes.first(std::min(k_entry_signature_preview_bytes, entry_bytes.size()));
    const auto classification_hint = classify_entry_hint(signature_bytes);
    const auto recognized = has_recognizable_signature(signature_bytes, classification_hint);

    container.entries.push_back(Win95PackContainerEntry{
        .index = index,
        .offset = offset,
        .size = size,
        .end_offset = end_offset.value(),
        .signature_hex = to_hex_signature(signature_bytes),
        .signature_ascii = to_ascii_signature(signature_bytes),
        .classification_hint = classification_hint,
        .has_recognizable_signature = recognized,
    });

    if (recognized) {
      ++container.summary.recognizable_signature_count;
    }
    const auto total_bytes = checked_add(container.summary.total_payload_bytes, size);
    if (!total_bytes.has_value()) {
      return {.error = make_invalid_container_error(
                  Win95PackContainerHeader::kHeaderSize + (index * Win95PackContainerHeader::kEntrySize),
                  Win95PackContainerHeader::kEntrySize,
                  bytes.size(),
                  "Total payload byte summary overflows host size")};
    }

    container.summary.total_payload_bytes = total_bytes.value();
    update_size_bucket(container.summary.size_buckets, size);
    ++magic_frequency_map[make_magic_key(signature_bytes)];

    if (size > 0) {
      ranges.push_back(RangeCheck{.index = index, .offset = offset, .end_offset = end_offset.value()});
    }
  }

  std::sort(ranges.begin(), ranges.end(), [](const RangeCheck& left, const RangeCheck& right) {
    if (left.offset == right.offset) {
      return left.index < right.index;
    }

    return left.offset < right.offset;
  });

  for (std::size_t index = 1; index < ranges.size(); ++index) {
    const auto& previous = ranges[index - 1];
    const auto& current = ranges[index];

    if (current.offset < previous.end_offset) {
      std::ostringstream message;
      message << "Entry " << current.index << " overlaps entry " << previous.index;
      const auto entry_record_offset = Win95PackContainerHeader::kHeaderSize + (current.index * Win95PackContainerHeader::kEntrySize);
      return {.error = make_invalid_container_error(entry_record_offset, Win95PackContainerHeader::kEntrySize, bytes.size(), message.str())};
    }
  }

  std::vector<Win95PackMagicFrequency> frequencies;
  frequencies.reserve(magic_frequency_map.size());
  for (const auto& [signature, count] : magic_frequency_map) {
    frequencies.push_back(Win95PackMagicFrequency{.signature = signature, .count = count});
  }

  std::stable_sort(frequencies.begin(), frequencies.end(), [](const auto& left, const auto& right) {
    if (left.count != right.count) {
      return left.count > right.count;
    }

    return left.signature < right.signature;
  });

  if (frequencies.size() > k_summary_magic_limit) {
    frequencies.resize(k_summary_magic_limit);
  }

  container.summary.magic_frequencies = std::move(frequencies);

  return {.value = std::move(container)};
}

ParseResult<Win95PackContainerResource> parse_win95_pack_container(std::span<const std::uint8_t> bytes) {
  return parse_win95_pack_container(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
}

ParseResult<Win95PackIlbmExtraction> extract_win95_pack_ilbm_entry(std::span<const std::byte> container_bytes,
                                                                    const Win95PackContainerResource& container,
                                                                    const std::size_t entry_index) {
  if (entry_index >= container.entries.size()) {
    std::ostringstream message;
    message << "Invalid PACK entry index " << entry_index << "; entry count=" << container.entries.size();
    return {.error = make_invalid_container_error(0, 0, container_bytes.size(), message.str())};
  }

  const auto& entry = container.entries[entry_index];
  if (entry.classification_hint != "possible-ilbm") {
    std::ostringstream message;
    message << "PACK entry " << entry_index << " is unsupported for extraction (class=" << entry.classification_hint
            << "): expected possible-ilbm";
    return {.error = make_invalid_container_error(entry.offset, entry.size, container_bytes.size(), message.str())};
  }

  const auto end_offset = checked_add(entry.offset, entry.size);
  if (!end_offset.has_value()) {
    std::ostringstream message;
    message << "PACK entry " << entry_index << " range overflows host size";
    return {.error = make_invalid_container_error(entry.offset, entry.size, container_bytes.size(), message.str())};
  }

  if (end_offset.value() > container_bytes.size()) {
    std::ostringstream message;
    message << "PACK entry " << entry_index << " exceeds container bounds during extraction";
    return {.error = make_out_of_bounds_container_error(entry.offset, entry.size, container_bytes.size(), message.str())};
  }

  const auto payload = container_bytes.subspan(entry.offset, entry.size);
  const auto parsed = parse_ilbm_image(payload);
  if (!parsed.ok()) {
    std::ostringstream message;
    message << "PACK entry " << entry_index << " failed ILBM validation: " << parsed.error->message;
    return {.error = make_invalid_container_error(entry.offset,
                                                  entry.size,
                                                  container_bytes.size(),
                                                  message.str())};
  }

  Win95PackIlbmExtraction extraction;
  extraction.entry = entry;
  extraction.payload_bytes.reserve(payload.size());
  for (const auto byte : payload) {
    extraction.payload_bytes.push_back(std::to_integer<std::uint8_t>(byte));
  }
  extraction.ilbm = parsed.value.value();
  return {.value = std::move(extraction)};
}

ParseResult<Win95PackIlbmExtraction> extract_win95_pack_ilbm_entry(std::span<const std::uint8_t> container_bytes,
                                                                    const Win95PackContainerResource& container,
                                                                    const std::size_t entry_index) {
  return extract_win95_pack_ilbm_entry(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(container_bytes.data()), container_bytes.size()),
      container,
      entry_index);
}

ParseResult<Win95PackTextExtraction> extract_win95_pack_text_entry(std::span<const std::byte> container_bytes,
                                                                   const Win95PackContainerResource& container,
                                                                   const std::size_t entry_index) {
  if (entry_index >= container.entries.size()) {
    std::ostringstream message;
    message << "Invalid PACK entry index " << entry_index << "; entry count=" << container.entries.size();
    return {.error = make_invalid_container_error(0, 0, container_bytes.size(), message.str())};
  }

  const auto& entry = container.entries[entry_index];
  if (entry.classification_hint != "text-like") {
    std::ostringstream message;
    message << "PACK entry " << entry_index << " is unsupported for text extraction (class=" << entry.classification_hint
            << "): expected text-like";
    return {.error = make_invalid_container_error(entry.offset, entry.size, container_bytes.size(), message.str())};
  }

  const auto end_offset = checked_add(entry.offset, entry.size);
  if (!end_offset.has_value()) {
    std::ostringstream message;
    message << "PACK entry " << entry_index << " range overflows host size";
    return {.error = make_invalid_container_error(entry.offset, entry.size, container_bytes.size(), message.str())};
  }

  if (end_offset.value() > container_bytes.size()) {
    std::ostringstream message;
    message << "PACK entry " << entry_index << " exceeds container bounds during extraction";
    return {.error = make_out_of_bounds_container_error(entry.offset, entry.size, container_bytes.size(), message.str())};
  }

  const auto payload = container_bytes.subspan(entry.offset, entry.size);
  std::string decoded_text;
  decoded_text.reserve(payload.size());
  std::vector<std::uint8_t> payload_bytes;
  payload_bytes.reserve(payload.size());
  for (const auto byte : payload) {
    const auto value = std::to_integer<unsigned char>(byte);
    payload_bytes.push_back(value);
    if (!is_supported_text_character(value)) {
      std::ostringstream message;
      message << "PACK entry " << entry_index << " failed text validation: unsupported byte 0x"
              << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(value);
      return {.error = make_invalid_container_error(entry.offset, entry.size, container_bytes.size(), message.str())};
    }

    decoded_text.push_back(static_cast<char>(value));
  }

  Win95PackTextExtraction extraction;
  extraction.entry = entry;
  extraction.payload_bytes = std::move(payload_bytes);
  extraction.decoded_text = std::move(decoded_text);
  extraction.character_count = extraction.decoded_text.size();
  extraction.line_count = count_text_lines(extraction.decoded_text);
  return {.value = std::move(extraction)};
}

ParseResult<Win95PackTextExtraction> extract_win95_pack_text_entry(std::span<const std::uint8_t> container_bytes,
                                                                   const Win95PackContainerResource& container,
                                                                   const std::size_t entry_index) {
  return extract_win95_pack_text_entry(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(container_bytes.data()), container_bytes.size()),
      container,
      entry_index);
}

ParseResult<Win95PackPl8Extraction> extract_win95_pack_pl8_entry(std::span<const std::byte> container_bytes,
                                                                  const Win95PackContainerResource& container,
                                                                  const std::size_t entry_index) {
  if (entry_index >= container.entries.size()) {
    std::ostringstream message;
    message << "Invalid PACK entry index " << entry_index << "; entry count=" << container.entries.size();
    return {.error = make_invalid_container_error(0, 0, container_bytes.size(), message.str())};
  }

  const auto& entry = container.entries[entry_index];
  if (!is_supported_pl8_like_candidate(entry)) {
    std::ostringstream message;
    message << "PACK entry " << entry_index
            << " is unsupported for PL8 extraction (class=" << entry.classification_hint << ", size=" << entry.size
            << "): expected opaque 768-byte palette-like payload";
    return {.error = make_invalid_container_error(entry.offset, entry.size, container_bytes.size(), message.str())};
  }

  const auto end_offset = checked_add(entry.offset, entry.size);
  if (!end_offset.has_value()) {
    std::ostringstream message;
    message << "PACK entry " << entry_index << " range overflows host size";
    return {.error = make_invalid_container_error(entry.offset, entry.size, container_bytes.size(), message.str())};
  }

  if (end_offset.value() > container_bytes.size()) {
    std::ostringstream message;
    message << "PACK entry " << entry_index << " exceeds container bounds during extraction";
    return {.error = make_out_of_bounds_container_error(entry.offset, entry.size, container_bytes.size(), message.str())};
  }

  const auto payload = container_bytes.subspan(entry.offset, entry.size);
  const auto parsed = parse_pl8_resource(payload);
  if (!parsed.ok()) {
    std::ostringstream message;
    message << "PACK entry " << entry_index << " failed PL8 validation: " << parsed.error->message;
    return {.error = make_invalid_container_error(entry.offset, entry.size, container_bytes.size(), message.str())};
  }

  Win95PackPl8Extraction extraction;
  extraction.entry = entry;
  extraction.payload_bytes.reserve(payload.size());
  for (const auto byte : payload) {
    extraction.payload_bytes.push_back(std::to_integer<std::uint8_t>(byte));
  }
  extraction.pl8 = parsed.value.value();
  return {.value = std::move(extraction)};
}

ParseResult<Win95PackPl8Extraction> extract_win95_pack_pl8_entry(std::span<const std::uint8_t> container_bytes,
                                                                  const Win95PackContainerResource& container,
                                                                  const std::size_t entry_index) {
  return extract_win95_pack_pl8_entry(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(container_bytes.data()), container_bytes.size()),
      container,
      entry_index);
}

Win95PackIlbmBatchResult analyze_win95_pack_ilbm_batch(std::span<const std::byte> container_bytes,
                                                        const Win95PackContainerResource& container) {
  Win95PackIlbmBatchResult result;
  result.total_entry_count = container.entries.size();
  std::map<std::string, std::size_t> failure_counts;

  for (const auto& entry : container.entries) {
    if (entry.classification_hint != "possible-ilbm") {
      continue;
    }

    ++result.candidate_entry_count;
    Win95PackIlbmBatchEntryResult entry_result{
        .entry_index = entry.index,
        .offset = entry.offset,
        .size = entry.size,
        .classification_hint = entry.classification_hint,
    };

    const auto extracted = extract_win95_pack_ilbm_entry(container_bytes, container, entry.index);
    if (!extracted.ok()) {
      ++result.failed_entry_count;
      entry_result.parse_success = false;
      const auto reason = compact_failure_reason(extracted.error->message);
      entry_result.failure_reason = reason;
      ++failure_counts[reason];
      result.entry_results.push_back(std::move(entry_result));
      continue;
    }

    ++result.parsed_entry_count;
    entry_result.parse_success = true;
    entry_result.width = extracted.value->ilbm.width;
    entry_result.height = extracted.value->ilbm.height;
    entry_result.palette_color_count = extracted.value->ilbm.palette_entries.size();
    result.entry_results.push_back(std::move(entry_result));
  }

  result.failure_reason_frequencies.reserve(failure_counts.size());
  for (const auto& [reason, count] : failure_counts) {
    result.failure_reason_frequencies.push_back(Win95PackMagicFrequency{
        .signature = reason,
        .count = count,
    });
  }
  std::stable_sort(result.failure_reason_frequencies.begin(), result.failure_reason_frequencies.end(), [](const auto& left, const auto& right) {
    if (left.count != right.count) {
      return left.count > right.count;
    }
    return left.signature < right.signature;
  });

  return result;
}

Win95PackIlbmBatchResult analyze_win95_pack_ilbm_batch(std::span<const std::uint8_t> container_bytes,
                                                        const Win95PackContainerResource& container) {
  return analyze_win95_pack_ilbm_batch(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(container_bytes.data()), container_bytes.size()),
      container);
}

Win95PackTextBatchResult analyze_win95_pack_text_batch(std::span<const std::byte> container_bytes,
                                                        const Win95PackContainerResource& container,
                                                        const std::size_t preview_character_limit) {
  Win95PackTextBatchResult result;
  result.total_entry_count = container.entries.size();
  std::map<std::string, std::size_t> failure_counts;

  for (const auto& entry : container.entries) {
    if (entry.classification_hint != "text-like") {
      continue;
    }

    ++result.candidate_entry_count;
    Win95PackTextBatchEntryResult entry_result{
        .entry_index = entry.index,
        .offset = entry.offset,
        .size = entry.size,
        .classification_hint = entry.classification_hint,
    };

    const auto extracted = extract_win95_pack_text_entry(container_bytes, container, entry.index);
    if (!extracted.ok()) {
      ++result.failed_entry_count;
      entry_result.decode_success = false;
      const auto reason = compact_failure_reason(extracted.error->message);
      entry_result.failure_reason = reason;
      ++failure_counts[reason];
      result.entry_results.push_back(std::move(entry_result));
      continue;
    }

    ++result.decoded_entry_count;
    entry_result.decode_success = true;
    entry_result.line_count = extracted.value->line_count;
    entry_result.character_count = extracted.value->character_count;
    bool truncated = false;
    entry_result.text_preview = make_text_preview(extracted.value->decoded_text, preview_character_limit, &truncated);
    result.entry_results.push_back(std::move(entry_result));
  }

  result.failure_reason_frequencies.reserve(failure_counts.size());
  for (const auto& [reason, count] : failure_counts) {
    result.failure_reason_frequencies.push_back(Win95PackMagicFrequency{
        .signature = reason,
        .count = count,
    });
  }
  std::stable_sort(result.failure_reason_frequencies.begin(), result.failure_reason_frequencies.end(), [](const auto& left, const auto& right) {
    if (left.count != right.count) {
      return left.count > right.count;
    }
    return left.signature < right.signature;
  });

  return result;
}

Win95PackTextBatchResult analyze_win95_pack_text_batch(std::span<const std::uint8_t> container_bytes,
                                                        const Win95PackContainerResource& container,
                                                        const std::size_t preview_character_limit) {
  return analyze_win95_pack_text_batch(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(container_bytes.data()), container_bytes.size()),
      container,
      preview_character_limit);
}

Win95PackIlbmIndex build_win95_pack_ilbm_success_index(const Win95PackIlbmBatchResult& batch_result) {
  Win95PackIlbmIndex index;
  index.total_entry_count = batch_result.total_entry_count;
  index.candidate_entry_count = batch_result.candidate_entry_count;

  for (const auto& entry_result : batch_result.entry_results) {
    if (!entry_result.parse_success) {
      continue;
    }

    index.successful_entries.push_back(Win95PackIlbmIndexEntry{
        .entry_index = entry_result.entry_index,
        .offset = entry_result.offset,
        .size = entry_result.size,
        .width = entry_result.width.value_or(0),
        .height = entry_result.height.value_or(0),
        .palette_color_count = entry_result.palette_color_count,
        .classification_hint = entry_result.classification_hint,
    });
  }

  index.successful_entry_count = index.successful_entries.size();
  return index;
}

Win95PackTextIndex build_win95_pack_text_success_index(const Win95PackTextBatchResult& batch_result) {
  Win95PackTextIndex index;
  index.total_entry_count = batch_result.total_entry_count;
  index.candidate_entry_count = batch_result.candidate_entry_count;

  for (const auto& entry_result : batch_result.entry_results) {
    if (!entry_result.decode_success) {
      continue;
    }

    index.successful_entries.push_back(Win95PackTextIndexEntry{
        .entry_index = entry_result.entry_index,
        .offset = entry_result.offset,
        .size = entry_result.size,
        .line_count = entry_result.line_count.value_or(0),
        .character_count = entry_result.character_count.value_or(0),
        .classification_hint = entry_result.classification_hint,
        .text_preview = entry_result.text_preview.value_or(""),
    });
  }

  index.successful_entry_count = index.successful_entries.size();
  return index;
}

Win95PackUnifiedSuccessIndex build_win95_pack_unified_success_index(std::span<const std::byte> container_bytes,
                                                                    const Win95PackContainerResource& container,
                                                                    const std::size_t preview_character_limit) {
  Win95PackUnifiedSuccessIndex unified;
  unified.summary.total_entry_count = container.entries.size();

  const auto ilbm_batch = analyze_win95_pack_ilbm_batch(container_bytes, container);
  const auto ilbm_index = build_win95_pack_ilbm_success_index(ilbm_batch);
  for (const auto& entry : ilbm_index.successful_entries) {
    unified.successful_entries.push_back(Win95PackUnifiedSuccessEntry{
        .entry_index = entry.entry_index,
        .offset = entry.offset,
        .size = entry.size,
        .family_kind = Win95PackKnownFamilyKind::Ilbm,
        .ilbm_width = entry.width,
        .ilbm_height = entry.height,
        .ilbm_palette_color_count = entry.palette_color_count,
    });
  }

  const auto text_batch = analyze_win95_pack_text_batch(container_bytes, container, preview_character_limit);
  const auto text_index = build_win95_pack_text_success_index(text_batch);
  for (const auto& entry : text_index.successful_entries) {
    unified.successful_entries.push_back(Win95PackUnifiedSuccessEntry{
        .entry_index = entry.entry_index,
        .offset = entry.offset,
        .size = entry.size,
        .family_kind = Win95PackKnownFamilyKind::Text,
        .text_line_count = entry.line_count,
        .text_character_count = entry.character_count,
        .text_preview = entry.text_preview,
    });
  }

  for (const auto& entry : container.entries) {
    const auto extracted = extract_win95_pack_pl8_entry(container_bytes, container, entry.index);
    if (!extracted.ok()) {
      continue;
    }

    unified.successful_entries.push_back(Win95PackUnifiedSuccessEntry{
        .entry_index = entry.index,
        .offset = entry.offset,
        .size = entry.size,
        .family_kind = Win95PackKnownFamilyKind::Pl8,
        .pl8_palette_entry_count = extracted.value->pl8.palette_entries.size(),
    });
  }

  std::stable_sort(unified.successful_entries.begin(), unified.successful_entries.end(), [](const auto& left, const auto& right) {
    if (left.entry_index != right.entry_index) {
      return left.entry_index < right.entry_index;
    }
    return left.offset < right.offset;
  });

  for (const auto& entry : unified.successful_entries) {
    if (entry.family_kind == Win95PackKnownFamilyKind::Ilbm) {
      ++unified.summary.ilbm_success_count;
    } else if (entry.family_kind == Win95PackKnownFamilyKind::Text) {
      ++unified.summary.text_success_count;
    } else if (entry.family_kind == Win95PackKnownFamilyKind::Pl8) {
      ++unified.summary.pl8_success_count;
    }
  }

  unified.summary.known_entry_count = unified.successful_entries.size();
  if (unified.summary.total_entry_count >= unified.summary.known_entry_count) {
    unified.summary.unknown_entry_count = unified.summary.total_entry_count - unified.summary.known_entry_count;
  }
  return unified;
}

Win95PackUnifiedSuccessIndex build_win95_pack_unified_success_index(std::span<const std::uint8_t> container_bytes,
                                                                    const Win95PackContainerResource& container,
                                                                    const std::size_t preview_character_limit) {
  return build_win95_pack_unified_success_index(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(container_bytes.data()), container_bytes.size()),
      container,
      preview_character_limit);
}

std::optional<Win95PackIlbmIndexEntry> find_win95_pack_ilbm_index_entry(const Win95PackIlbmIndex& index,
                                                                         const std::size_t entry_index) {
  for (const auto& entry : index.successful_entries) {
    if (entry.entry_index == entry_index) {
      return entry;
    }
  }

  return std::nullopt;
}

std::optional<Win95PackTextIndexEntry> find_win95_pack_text_index_entry(const Win95PackTextIndex& index,
                                                                         const std::size_t entry_index) {
  for (const auto& entry : index.successful_entries) {
    if (entry.entry_index == entry_index) {
      return entry;
    }
  }

  return std::nullopt;
}

ProbeWin95DataContainerResult probe_win95_data_container_file(const std::filesystem::path& data_root,
                                                              const std::string& candidate_path,
                                                              const std::size_t max_file_load_bytes) {
  const auto relative = std::filesystem::path(candidate_path).lexically_normal();
  const auto absolute = relative.is_absolute() ? relative : (data_root / relative);

  const auto loaded = load_file_to_memory(absolute, max_file_load_bytes);
  if (!loaded.ok()) {
    return {.error = Win95DataContainerProbeError{.requested_path = relative, .message = loaded.error->message}};
  }

  const auto parsed = parse_win95_pack_container(loaded.value->bytes);
  if (!parsed.ok()) {
    return {.error = Win95DataContainerProbeError{.requested_path = relative, .message = parsed.error->message}};
  }

  return {.value = parsed.value};
}

std::string format_win95_data_container_report(const Win95PackContainerResource& resource,
                                               std::string_view source_label,
                                               const Win95PackReportOptions options) {
  std::ostringstream output;
  output << "# Caesar II Win95 PACK Container Report\n";
  if (!source_label.empty()) {
    output << "source: " << source_label << "\n";
  }

  output << "signature: " << resource.header.signature << "\n";
  output << "entry_count: " << resource.header.entry_count << "\n";
  output << "entry_table_offset: " << resource.header.entry_table_offset << "\n";
  output << "entry_table_size: " << resource.header.entry_table_size << "\n";
  output << "total_payload_bytes: " << resource.summary.total_payload_bytes << "\n";
  output << "recognizable_signatures: " << resource.summary.recognizable_signature_count << "\n";
  output << "size_buckets: "
         << "0-255=" << resource.summary.size_buckets.tiny_bytes_0_to_255 << " "
         << "256-4095=" << resource.summary.size_buckets.small_bytes_256_to_4095 << " "
         << "4096-65535=" << resource.summary.size_buckets.medium_bytes_4096_to_65535 << " "
         << "65536+=" << resource.summary.size_buckets.large_bytes_65536_plus << "\n";

  output << "signature_frequency:\n";
  for (const auto& frequency : resource.summary.magic_frequencies) {
    output << "  - signature: " << frequency.signature << " count=" << frequency.count << "\n";
  }
  if (resource.summary.magic_frequencies.empty()) {
    output << "  - (none)\n";
  }

  const auto entry_limit = options.include_all_entries
                               ? resource.entries.size()
                               : std::min<std::size_t>(options.preview_entry_limit, resource.entries.size());
  output << "entries_preview: showing " << entry_limit << " of " << resource.entries.size() << "\n";
  for (std::size_t index = 0; index < entry_limit; ++index) {
    const auto& entry = resource.entries[index];
    output << "  - index: " << entry.index << " offset=" << entry.offset << " size=" << entry.size
           << " end_offset=" << entry.end_offset << " signature_hex=" << (entry.signature_hex.empty() ? "(empty)" : entry.signature_hex)
           << " signature_ascii=" << (entry.signature_ascii.empty() ? "(empty)" : entry.signature_ascii)
           << " class=" << entry.classification_hint
           << " recognizable=" << (entry.has_recognizable_signature ? "yes" : "no") << "\n";
  }

  if (entry_limit < resource.entries.size()) {
    output << "entries_preview_truncated: yes\n";
  }

  return output.str();
}

std::string format_win95_pack_ilbm_batch_report(const Win95PackIlbmBatchResult& result,
                                                 std::string_view source_label,
                                                 const Win95PackIlbmBatchReportOptions options) {
  std::ostringstream output;
  output << "# Caesar II Win95 PACK ILBM Batch Report\n";
  if (!source_label.empty()) {
    output << "source: " << source_label << "\n";
  }

  output << "total_entries: " << result.total_entry_count << "\n";
  output << "possible_ilbm_entries: " << result.candidate_entry_count << "\n";
  output << "parsed_ilbm_entries: " << result.parsed_entry_count << "\n";
  output << "failed_ilbm_entries: " << result.failed_entry_count << "\n";

  output << "failure_reasons:\n";
  if (result.failure_reason_frequencies.empty()) {
    output << "  - (none)\n";
  } else {
    for (const auto& frequency : result.failure_reason_frequencies) {
      output << "  - reason: " << frequency.signature << " count=" << frequency.count << "\n";
    }
  }

  const auto entry_limit = options.include_all_entries
                               ? result.entry_results.size()
                               : std::min<std::size_t>(options.preview_entry_limit, result.entry_results.size());
  output << "ilbm_entries_preview: showing " << entry_limit << " of " << result.entry_results.size() << "\n";
  for (std::size_t index = 0; index < entry_limit; ++index) {
    const auto& entry = result.entry_results[index];
    output << "  - index: " << entry.entry_index
           << " offset=" << entry.offset
           << " size=" << entry.size
           << " class=" << entry.classification_hint
           << " status=" << (entry.parse_success ? "parsed" : "failed");
    if (entry.parse_success) {
      output << " width=" << entry.width.value_or(0)
             << " height=" << entry.height.value_or(0)
             << " palette_colors=" << entry.palette_color_count.value_or(0);
    } else {
      output << " reason=" << entry.failure_reason.value_or("unknown");
    }

    output << "\n";
  }

  if (entry_limit < result.entry_results.size()) {
    output << "ilbm_entries_preview_truncated: yes\n";
  }

  return output.str();
}

std::string format_win95_pack_ilbm_index_report(const Win95PackIlbmIndex& index,
                                                std::string_view source_label,
                                                const Win95PackIlbmIndexReportOptions options) {
  std::ostringstream output;
  output << "# Caesar II Win95 PACK ILBM Success Index Report\n";
  if (!source_label.empty()) {
    output << "source: " << source_label << "\n";
  }

  output << "total_entries: " << index.total_entry_count << "\n";
  output << "possible_ilbm_entries: " << index.candidate_entry_count << "\n";
  output << "successful_ilbm_entries: " << index.successful_entry_count << "\n";

  const auto entry_limit = options.include_all_entries
                               ? index.successful_entries.size()
                               : std::min<std::size_t>(options.preview_entry_limit, index.successful_entries.size());
  output << "successful_entries_preview: showing " << entry_limit << " of " << index.successful_entries.size() << "\n";

  for (std::size_t current = 0; current < entry_limit; ++current) {
    const auto& entry = index.successful_entries[current];
    output << "  - index: " << entry.entry_index
           << " offset=" << entry.offset
           << " size=" << entry.size
           << " class=" << entry.classification_hint
           << " width=" << entry.width
           << " height=" << entry.height
           << " palette_colors=" << entry.palette_color_count.value_or(0)
           << "\n";
  }

  if (entry_limit < index.successful_entries.size()) {
    output << "successful_entries_preview_truncated: yes\n";
  }

  return output.str();
}

std::string format_win95_pack_text_batch_report(const Win95PackTextBatchResult& result,
                                                std::string_view source_label,
                                                const Win95PackTextBatchReportOptions options) {
  std::ostringstream output;
  output << "# Caesar II Win95 PACK Text Batch Report\n";
  if (!source_label.empty()) {
    output << "source: " << source_label << "\n";
  }

  output << "total_entries: " << result.total_entry_count << "\n";
  output << "text_like_entries: " << result.candidate_entry_count << "\n";
  output << "decoded_text_entries: " << result.decoded_entry_count << "\n";
  output << "failed_text_entries: " << result.failed_entry_count << "\n";

  output << "failure_reasons:\n";
  if (result.failure_reason_frequencies.empty()) {
    output << "  - (none)\n";
  } else {
    for (const auto& frequency : result.failure_reason_frequencies) {
      output << "  - reason: " << frequency.signature << " count=" << frequency.count << "\n";
    }
  }

  const auto entry_limit = options.include_all_entries
                               ? result.entry_results.size()
                               : std::min<std::size_t>(options.preview_entry_limit, result.entry_results.size());
  output << "text_entries_preview: showing " << entry_limit << " of " << result.entry_results.size() << "\n";
  for (std::size_t current = 0; current < entry_limit; ++current) {
    const auto& entry = result.entry_results[current];
    output << "  - index: " << entry.entry_index
           << " offset=" << entry.offset
           << " size=" << entry.size
           << " class=" << entry.classification_hint
           << " status=" << (entry.decode_success ? "decoded" : "failed");
    if (entry.decode_success) {
      output << " lines=" << entry.line_count.value_or(0)
             << " chars=" << entry.character_count.value_or(0)
             << " preview=" << entry.text_preview.value_or("");
    } else {
      output << " reason=" << entry.failure_reason.value_or("unknown");
    }
    output << "\n";
  }

  if (entry_limit < result.entry_results.size()) {
    output << "text_entries_preview_truncated: yes\n";
  }

  return output.str();
}

std::string format_win95_pack_text_index_report(const Win95PackTextIndex& index,
                                                std::string_view source_label,
                                                const Win95PackTextIndexReportOptions options) {
  std::ostringstream output;
  output << "# Caesar II Win95 PACK Text Success Index Report\n";
  if (!source_label.empty()) {
    output << "source: " << source_label << "\n";
  }

  output << "total_entries: " << index.total_entry_count << "\n";
  output << "text_like_entries: " << index.candidate_entry_count << "\n";
  output << "successful_text_entries: " << index.successful_entry_count << "\n";

  const auto entry_limit = options.include_all_entries
                               ? index.successful_entries.size()
                               : std::min<std::size_t>(options.preview_entry_limit, index.successful_entries.size());
  output << "successful_entries_preview: showing " << entry_limit << " of " << index.successful_entries.size() << "\n";
  for (std::size_t current = 0; current < entry_limit; ++current) {
    const auto& entry = index.successful_entries[current];
    output << "  - index: " << entry.entry_index
           << " offset=" << entry.offset
           << " size=" << entry.size
           << " class=" << entry.classification_hint
           << " lines=" << entry.line_count
           << " chars=" << entry.character_count
           << " preview=" << entry.text_preview
           << "\n";
  }

  if (entry_limit < index.successful_entries.size()) {
    output << "successful_entries_preview_truncated: yes\n";
  }

  return output.str();
}

std::string format_win95_pack_text_report(const Win95PackTextExtraction& extraction,
                                          std::string_view source_label,
                                          const Win95PackTextReportOptions options) {
  std::ostringstream output;
  output << "# Caesar II Win95 PACK Text Entry Report\n";
  if (!source_label.empty()) {
    output << "source: " << source_label << "\n";
  }

  output << "entry_index: " << extraction.entry.index << "\n";
  output << "offset: " << extraction.entry.offset << "\n";
  output << "size: " << extraction.entry.size << "\n";
  output << "classification: " << extraction.entry.classification_hint << "\n";
  output << "line_count: " << extraction.line_count << "\n";
  output << "character_count: " << extraction.character_count << "\n";
  bool truncated = false;
  output << "text_preview: "
         << make_text_preview(extraction.decoded_text, options.preview_character_limit, &truncated)
         << "\n";
  output << "text_preview_truncated: " << (truncated ? "yes" : "no") << "\n";
  return output.str();
}

std::string format_win95_pack_unified_success_index_report(const Win95PackUnifiedSuccessIndex& index,
                                                           std::string_view source_label,
                                                           const Win95PackUnifiedSuccessReportOptions options) {
  std::ostringstream output;
  output << "# Caesar II Win95 PACK Unified Success Index Report\n";
  if (!source_label.empty()) {
    output << "source: " << source_label << "\n";
  }

  output << "total_entries: " << index.summary.total_entry_count << "\n";
  output << "known_supported_entries: " << index.summary.known_entry_count << "\n";
  output << "unknown_entries: " << index.summary.unknown_entry_count << "\n";
  output << "known_ratio: " << index.summary.known_entry_count << "/" << index.summary.total_entry_count << "\n";
  output << "family_success_counts: ilbm=" << index.summary.ilbm_success_count
         << " text=" << index.summary.text_success_count
         << " pl8=" << index.summary.pl8_success_count << "\n";

  const auto entry_limit = options.include_all_entries
                               ? index.successful_entries.size()
                               : std::min<std::size_t>(options.preview_entry_limit, index.successful_entries.size());
  output << "successful_entries_preview: showing " << entry_limit << " of " << index.successful_entries.size() << "\n";
  for (std::size_t current = 0; current < entry_limit; ++current) {
    const auto& entry = index.successful_entries[current];
    output << "  - index: " << entry.entry_index
           << " offset=" << entry.offset
           << " size=" << entry.size
           << " family=" << to_family_label(entry.family_kind);
    if (entry.family_kind == Win95PackKnownFamilyKind::Ilbm) {
      output << " width=" << entry.ilbm_width.value_or(0)
             << " height=" << entry.ilbm_height.value_or(0)
             << " palette_colors=" << entry.ilbm_palette_color_count.value_or(0);
    } else if (entry.family_kind == Win95PackKnownFamilyKind::Text) {
      output << " lines=" << entry.text_line_count.value_or(0)
             << " chars=" << entry.text_character_count.value_or(0)
             << " preview=" << entry.text_preview.value_or("");
    } else if (entry.family_kind == Win95PackKnownFamilyKind::Pl8) {
      output << " palette_entries=" << entry.pl8_palette_entry_count.value_or(0)
             << " layout=simple-pl8";
    }
    output << "\n";
  }

  if (entry_limit < index.successful_entries.size()) {
    output << "successful_entries_preview_truncated: yes\n";
  }

  return output.str();
}

}  // namespace romulus::data
