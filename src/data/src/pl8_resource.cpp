#include "romulus/data/pl8_resource.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string_view>

#include "romulus/data/file_loader.h"

namespace romulus::data {
namespace {

[[nodiscard]] ParseError make_invalid_pl8_error(std::size_t offset,
                                                std::size_t requested_bytes,
                                                std::size_t buffer_size,
                                                const std::string& message) {
  return make_invalid_format_error(offset, requested_bytes, buffer_size, message);
}

[[nodiscard]] std::string format_hex_byte(std::uint8_t value) {
  std::ostringstream output;
  output << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(value);
  return output.str();
}

[[nodiscard]] std::string format_compact_palette_summary(std::span<const PaletteEntry> entries) {
  std::ostringstream output;
  output << "palette_entries=" << Pl8Resource::kSupportedEntryCount << " preview=[";

  for (std::size_t index = 0; index < entries.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }

    const auto& entry = entries[index];
    output << index << ":(" << static_cast<unsigned int>(entry.red) << "," << static_cast<unsigned int>(entry.green)
           << "," << static_cast<unsigned int>(entry.blue) << ")";
  }

  output << "]";
  return output.str();
}

}  // namespace

ParseResult<Pl8Resource> parse_pl8_resource(std::span<const std::byte> bytes) {
  BinaryReader reader(bytes);

  if (bytes.size() != Pl8Resource::kSupportedPayloadSize) {
    std::ostringstream message;
    message << "Unsupported PL8 layout: expected exactly " << Pl8Resource::kSupportedPayloadSize
            << " bytes for 256 RGB triplets, got " << bytes.size();
    return {.error = make_invalid_pl8_error(0, bytes.size(), bytes.size(), message.str())};
  }

  std::vector<PaletteEntry> entries;
  entries.reserve(Pl8Resource::kSupportedEntryCount);

  for (std::size_t index = 0; index < Pl8Resource::kSupportedEntryCount; ++index) {
    const auto red = reader.read_u8();
    if (!red.ok()) {
      return {.error = red.error};
    }

    const auto green = reader.read_u8();
    if (!green.ok()) {
      return {.error = green.error};
    }

    const auto blue = reader.read_u8();
    if (!blue.ok()) {
      return {.error = blue.error};
    }

    entries.push_back(PaletteEntry{.red = red.value.value(), .green = green.value.value(), .blue = blue.value.value()});
  }

  if (reader.remaining() != 0) {
    return {.error = make_invalid_pl8_error(
                reader.tell(),
                reader.remaining(),
                bytes.size(),
                "PL8 parser invariant failed: trailing bytes remained after decoding fixed payload")};
  }

  Pl8Resource resource;
  resource.payload_offset = 0;
  resource.payload_size = bytes.size();
  resource.palette_entries = std::move(entries);

  return {.value = std::move(resource)};
}

ParseResult<Pl8Resource> parse_pl8_resource(std::span<const std::uint8_t> bytes) {
  return parse_pl8_resource(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
}

std::string format_pl8_report(const Pl8Resource& resource, std::size_t max_palette_entries) {
  std::ostringstream output;
  output << "# Caesar II Win95 PL8 Report\n";
  output << "supported_layout: raw_rgb_triplets_256\n";
  output << "payload_offset: " << resource.payload_offset << "\n";
  output << "payload_size: " << resource.payload_size << "\n";
  output << "palette_entries: " << resource.palette_entries.size() << "\n";

  const auto preview_count = std::min(max_palette_entries, resource.palette_entries.size());
  output << "palette_preview_count: " << preview_count << "\n";
  output << "palette_preview:\n";

  for (std::size_t i = 0; i < preview_count; ++i) {
    const auto& entry = resource.palette_entries[i];
    output << "  - index: " << i << " rgb=(" << static_cast<unsigned int>(entry.red) << ","
           << static_cast<unsigned int>(entry.green) << "," << static_cast<unsigned int>(entry.blue) << ")"
           << " hex=(" << format_hex_byte(entry.red) << "," << format_hex_byte(entry.green) << ","
           << format_hex_byte(entry.blue) << ")\n";
  }

  return output.str();
}

ProbePl8BatchResult probe_pl8_files(const std::filesystem::path& data_root,
                                    const std::vector<std::string>& candidates,
                                    const std::size_t max_file_load_bytes) {
  Pl8BatchReport report;
  report.files.reserve(candidates.size());

  for (const auto& candidate : candidates) {
    std::filesystem::path relative_path = std::filesystem::path(candidate).lexically_normal();
    const auto absolute_path = relative_path.is_absolute() ? relative_path : (data_root / relative_path);

    const auto loaded = load_file_to_memory(absolute_path, max_file_load_bytes);
    if (!loaded.ok()) {
      Pl8BatchProbeError error;
      error.requested_path = relative_path;
      error.message = loaded.error->message;
      return {.error = error};
    }

    Pl8BatchFileReport file;
    file.relative_path = relative_path;
    file.size_bytes = loaded.value->bytes.size();

    if (file.size_bytes == Pl8Resource::kSupportedPayloadSize) {
      const auto parsed = parse_pl8_resource(loaded.value->bytes);
      if (!parsed.ok()) {
        Pl8BatchProbeError error;
        error.requested_path = relative_path;
        error.message = parsed.error->message;
        return {.error = error};
      }

      file.matches_supported_palette_layout = true;
      const auto preview_count = std::min<std::size_t>(3, parsed.value->palette_entries.size());
      file.palette_preview_entries.reserve(preview_count);
      for (std::size_t index = 0; index < preview_count; ++index) {
        file.palette_preview_entries.push_back(parsed.value->palette_entries[index]);
      }
      file.summary = format_compact_palette_summary(file.palette_preview_entries);
    } else {
      std::ostringstream summary;
      summary << "unsupported_layout: payload_size=" << file.size_bytes
              << " expected_payload_size=" << Pl8Resource::kSupportedPayloadSize;
      file.summary = summary.str();
    }

    report.files.push_back(std::move(file));
  }

  return {.value = std::move(report)};
}

std::string format_pl8_batch_report(const Pl8BatchReport& report, const std::size_t max_palette_entries) {
  std::ostringstream output;
  output << "# Caesar II Win95 PL8 Batch Report\n";

  for (const auto& file : report.files) {
    output << "\n[file]\n";
    output << "path: " << file.relative_path.generic_string() << "\n";
    output << "size_bytes: " << file.size_bytes << "\n";
    output << "matches_supported_palette_layout: " << (file.matches_supported_palette_layout ? "yes" : "no") << "\n";

    if (!file.matches_supported_palette_layout) {
      output << "unsupported_summary: " << file.summary << "\n";
      continue;
    }

    const auto preview_count = std::min(max_palette_entries, file.palette_preview_entries.size());
    output << "palette_summary: "
           << format_compact_palette_summary(std::span<const PaletteEntry>(file.palette_preview_entries.data(),
                                                                           preview_count))
           << "\n";
  }

  return output.str();
}

}  // namespace romulus::data
