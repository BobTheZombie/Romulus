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

}  // namespace romulus::data
