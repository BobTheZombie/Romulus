#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "romulus/data/file_loader.h"

namespace romulus::data {

struct DosMzHeaderSummary {
  std::uint16_t bytes_in_last_page = 0;
  std::uint16_t pages_in_file = 0;
  std::uint16_t relocation_table_offset = 0;
  std::uint32_t pe_header_offset = 0;
};

struct BinaryProbeReport {
  std::string source;
  std::size_t size_bytes = 0;
  std::string signature_hex;
  std::optional<std::uint16_t> first_u16_le;
  std::optional<std::uint32_t> first_u32_le;
  std::optional<DosMzHeaderSummary> dos_mz_header;
};

[[nodiscard]] BinaryProbeReport probe_loaded_binary(const LoadedFile& file);
[[nodiscard]] std::string format_binary_probe_report(const BinaryProbeReport& report);

}  // namespace romulus::data
