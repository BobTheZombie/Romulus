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

struct PeResourceDirectoryEntry {
  std::uint32_t raw_name_or_id = 0;
  bool uses_string_name = false;
  std::string string_name;
  std::uint32_t numeric_id = 0;
  bool points_to_subdirectory = false;
  std::uint32_t child_offset = 0;
};

struct PeResourceLeaf {
  std::uint32_t type_id = 0;
  std::string type_label;
  bool type_uses_string_name = false;
  std::string type_string_name;
  std::uint32_t name_id = 0;
  bool name_uses_string_name = false;
  std::string name_string;
  std::uint32_t language_id = 0;
  std::uint32_t data_rva = 0;
  std::uint32_t data_size = 0;
  std::size_t data_file_offset = 0;
};

struct PeResourceTypeSummary {
  std::uint32_t type_id = 0;
  std::string type_label;
  bool type_uses_string_name = false;
  std::string type_string_name;
  std::size_t entry_count = 0;
  std::size_t leaf_count = 0;
};

struct PeResourceTree {
  std::vector<PeResourceDirectoryEntry> top_level_entries;
  std::vector<PeResourceLeaf> leaves;
};

struct PeResourceSectionReport {
  bool has_resources = false;
  std::uint32_t resource_rva = 0;
  std::uint32_t resource_size = 0;
  std::size_t top_level_type_count = 0;
  std::size_t leaf_count = 0;
  std::vector<PeResourceTypeSummary> per_type_summary;
  PeResourceTree tree;
};

struct PeVersionResourceStringValue {
  std::string key;
  std::string value;
};

struct PeVersionResource {
  PeResourceLeaf source_leaf;
  bool has_fixed_file_info = false;
  std::uint32_t file_version_ms = 0;
  std::uint32_t file_version_ls = 0;
  std::uint32_t product_version_ms = 0;
  std::uint32_t product_version_ls = 0;
  std::vector<PeVersionResourceStringValue> string_values;
};

struct PeStringTableEntry {
  std::uint32_t string_id = 0;
  std::string text;
};

struct PeStringTableResource {
  PeResourceLeaf source_leaf;
  std::vector<PeStringTableEntry> entries;
};

struct PeResourcePayloadSkip {
  PeResourceLeaf source_leaf;
  std::string reason;
};

struct PeResourcePayloadReport {
  std::vector<PeVersionResource> version_resources;
  std::vector<PeStringTableResource> string_table_resources;
  std::vector<PeResourcePayloadSkip> skipped_resources;
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
  PeResourceSectionReport resource_report;
};

[[nodiscard]] ParseResult<PeExeResource> parse_pe_exe_resource(std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<PeExeResource> parse_pe_exe_resource(std::span<const std::uint8_t> bytes);
[[nodiscard]] ParseResult<PeResourcePayloadReport> decode_pe_resource_payloads(std::span<const std::byte> bytes,
                                                                               const PeExeResource& resource);
[[nodiscard]] ParseResult<PeResourcePayloadReport> decode_pe_resource_payloads(std::span<const std::uint8_t> bytes,
                                                                               const PeExeResource& resource);
[[nodiscard]] std::string format_pe_exe_report(const PeExeResource& resource);
[[nodiscard]] std::string format_pe_resource_report(const PeResourceSectionReport& resource_report);
[[nodiscard]] std::string format_pe_version_resource_report(const PeResourcePayloadReport& payload_report);
[[nodiscard]] std::string format_pe_string_table_report(const PeResourcePayloadReport& payload_report);
[[nodiscard]] std::string format_pe_resource_payload_report(const PeResourcePayloadReport& payload_report);

}  // namespace romulus::data
