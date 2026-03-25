#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "romulus/data/binary_reader.h"

namespace romulus::data {

struct PeImportDescriptor {
  std::string dll_name;
  std::vector<std::string> imported_functions;
};

struct PeExeResource {
  std::uint32_t image_base = 0;
  std::uint32_t entry_point_rva = 0;
  std::uint16_t subsystem = 0;
  std::uint16_t section_count = 0;
  std::uint32_t timestamp = 0;
  bool has_resources = false;
  bool has_relocations = false;
  std::vector<PeImportDescriptor> imports;
};

[[nodiscard]] ParseResult<PeExeResource> parse_pe_exe_resource(std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<PeExeResource> parse_pe_exe_resource(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::string format_pe_exe_report(const PeExeResource& resource);

}  // namespace romulus::data
