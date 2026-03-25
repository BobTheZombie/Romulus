#include "romulus/data/win95_data_container.h"

#include <algorithm>
#include <limits>
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

  std::vector<RangeCheck> ranges;
  ranges.reserve(static_cast<std::size_t>(entry_count.value.value()));

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

    container.entries.push_back(Win95PackContainerEntry{
        .index = index,
        .offset = offset,
        .size = size,
        .end_offset = end_offset.value(),
    });

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

  return {.value = std::move(container)};
}

ParseResult<Win95PackContainerResource> parse_win95_pack_container(std::span<const std::uint8_t> bytes) {
  return parse_win95_pack_container(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
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

std::string format_win95_data_container_report(const Win95PackContainerResource& resource, std::string_view source_label) {
  std::ostringstream output;
  output << "# Caesar II Win95 PACK Container Report\n";
  if (!source_label.empty()) {
    output << "source: " << source_label << "\n";
  }

  output << "signature: " << resource.header.signature << "\n";
  output << "entry_count: " << resource.header.entry_count << "\n";
  output << "entry_table_offset: " << resource.header.entry_table_offset << "\n";
  output << "entry_table_size: " << resource.header.entry_table_size << "\n";

  output << "entries:\n";
  for (const auto& entry : resource.entries) {
    output << "  - index: " << entry.index << " offset=" << entry.offset << " size=" << entry.size
           << " end_offset=" << entry.end_offset << "\n";
  }

  return output.str();
}

}  // namespace romulus::data
