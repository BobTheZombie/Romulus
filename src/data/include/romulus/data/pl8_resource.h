#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "romulus/data/binary_reader.h"
#include "romulus/data/palette.h"

namespace romulus::data {

struct Pl8Resource {
  static constexpr std::size_t kSupportedEntryCount = 256;
  static constexpr std::size_t kBytesPerEntry = 3;
  static constexpr std::size_t kSupportedPayloadSize = kSupportedEntryCount * kBytesPerEntry;

  std::size_t payload_offset = 0;
  std::size_t payload_size = 0;
  std::vector<PaletteEntry> palette_entries;
};

struct Pl8BatchFileReport {
  std::filesystem::path relative_path;
  std::size_t size_bytes = 0;
  bool matches_supported_palette_layout = false;
  std::vector<PaletteEntry> palette_preview_entries;
  std::string summary;
};

struct Pl8BatchReport {
  std::vector<Pl8BatchFileReport> files;
};

struct Pl8BatchProbeError {
  std::filesystem::path requested_path;
  std::string message;
};

struct ProbePl8BatchResult {
  std::optional<Pl8BatchReport> value;
  std::optional<Pl8BatchProbeError> error;

  [[nodiscard]] bool ok() const {
    return value.has_value();
  }
};

[[nodiscard]] ParseResult<Pl8Resource> parse_pl8_resource(std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<Pl8Resource> parse_pl8_resource(std::span<const std::uint8_t> bytes);

[[nodiscard]] std::string format_pl8_report(const Pl8Resource& resource, std::size_t max_palette_entries = 16);
[[nodiscard]] ProbePl8BatchResult probe_pl8_files(const std::filesystem::path& data_root,
                                                  const std::vector<std::string>& candidates,
                                                  std::size_t max_file_load_bytes = 8 * 1024 * 1024);
[[nodiscard]] std::string format_pl8_batch_report(const Pl8BatchReport& report, std::size_t max_palette_entries = 3);

}  // namespace romulus::data
