#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "romulus/data/binary_reader.h"

namespace romulus::data {

struct Win95PackContainerHeader {
  static constexpr std::size_t kHeaderSize = 8;
  static constexpr std::size_t kEntrySize = 8;

  std::string signature;
  std::uint32_t entry_count = 0;
  std::size_t entry_table_offset = kHeaderSize;
  std::size_t entry_table_size = 0;
};

struct Win95PackContainerEntry {
  std::size_t index = 0;
  std::size_t offset = 0;
  std::size_t size = 0;
  std::size_t end_offset = 0;
};

struct Win95PackContainerResource {
  Win95PackContainerHeader header;
  std::vector<Win95PackContainerEntry> entries;
};

struct Win95DataContainerProbeError {
  std::filesystem::path requested_path;
  std::string message;
};

struct ProbeWin95DataContainerResult {
  std::optional<Win95PackContainerResource> value;
  std::optional<Win95DataContainerProbeError> error;

  [[nodiscard]] bool ok() const {
    return value.has_value();
  }
};

constexpr std::size_t k_default_win95_container_max_file_load_bytes = 64 * 1024 * 1024;

[[nodiscard]] ParseResult<Win95PackContainerResource> parse_win95_pack_container(std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<Win95PackContainerResource> parse_win95_pack_container(std::span<const std::uint8_t> bytes);

[[nodiscard]] ProbeWin95DataContainerResult probe_win95_data_container_file(
    const std::filesystem::path& data_root,
    const std::string& candidate_path,
    std::size_t max_file_load_bytes = k_default_win95_container_max_file_load_bytes);

[[nodiscard]] std::string format_win95_data_container_report(const Win95PackContainerResource& resource,
                                                             std::string_view source_label = "");

}  // namespace romulus::data
