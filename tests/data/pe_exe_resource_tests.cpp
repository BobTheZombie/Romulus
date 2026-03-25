#include "romulus/data/pe_exe_resource.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int assert_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    return 1;
  }
  return 0;
}

void set_u16_le(std::vector<std::uint8_t>& out, std::size_t offset, std::uint16_t value) {
  out[offset + 0] = static_cast<std::uint8_t>(value & 0xFFU);
  out[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void set_u32_le(std::vector<std::uint8_t>& out, std::size_t offset, std::uint32_t value) {
  out[offset + 0] = static_cast<std::uint8_t>(value & 0xFFU);
  out[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
  out[offset + 2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
  out[offset + 3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

void set_ascii(std::vector<std::uint8_t>& out, std::size_t offset, const std::string& text) {
  for (std::size_t i = 0; i < text.size(); ++i) {
    out[offset + i] = static_cast<std::uint8_t>(text[i]);
  }
}

std::vector<std::uint8_t> make_minimal_valid_pe_fixture() {
  std::vector<std::uint8_t> bytes(0x600, 0x00);
  bytes[0] = 'M';
  bytes[1] = 'Z';
  set_u32_le(bytes, 0x3C, 0x80);

  bytes[0x80] = 'P';
  bytes[0x81] = 'E';

  set_u16_le(bytes, 0x84, 0x014C);
  set_u16_le(bytes, 0x86, 2);
  set_u32_le(bytes, 0x88, 0x5F3759DF);
  set_u16_le(bytes, 0x94, 224);
  set_u16_le(bytes, 0x96, 0x0102);

  const std::size_t opt = 0x98;
  set_u16_le(bytes, opt + 0, 0x10B);
  set_u32_le(bytes, opt + 16, 0x1010);
  set_u32_le(bytes, opt + 28, 0x400000);
  set_u16_le(bytes, opt + 68, 2);
  set_u32_le(bytes, opt + 92, 16);

  set_u32_le(bytes, opt + 96 + (1 * 8) + 0, 0x2000);
  set_u32_le(bytes, opt + 96 + (1 * 8) + 4, 0x40);
  set_u32_le(bytes, opt + 96 + (2 * 8) + 0, 0x2090);
  set_u32_le(bytes, opt + 96 + (2 * 8) + 4, 0x10);

  const std::size_t sec = 0x178;
  set_ascii(bytes, sec + 0, ".text");
  set_u32_le(bytes, sec + 8, 0x200);
  set_u32_le(bytes, sec + 12, 0x1000);
  set_u32_le(bytes, sec + 16, 0x200);
  set_u32_le(bytes, sec + 20, 0x200);

  set_ascii(bytes, sec + 40, ".rdata");
  set_u32_le(bytes, sec + 48, 0x200);
  set_u32_le(bytes, sec + 52, 0x2000);
  set_u32_le(bytes, sec + 56, 0x200);
  set_u32_le(bytes, sec + 60, 0x400);

  set_u32_le(bytes, 0x400, 0x2040);
  set_u32_le(bytes, 0x40C, 0x2060);
  set_u32_le(bytes, 0x440, 0x2070);
  set_u32_le(bytes, 0x444, 0x80001234);
  set_u32_le(bytes, 0x448, 0x2080);

  set_ascii(bytes, 0x460, "KERNEL32.dll");
  set_ascii(bytes, 0x472, "CreateFileA");
  set_ascii(bytes, 0x482, "ReadFile");

  return bytes;
}

int test_parse_valid_minimal_pe() {
  const auto parsed = romulus::data::parse_pe_exe_resource(make_minimal_valid_pe_fixture());
  if (assert_true(parsed.ok(), "valid PE fixture should parse") != 0) {
    return 1;
  }

  const auto& resource = parsed.value.value();
  if (assert_true(resource.image_base == 0x400000, "image base should decode") != 0) {
    return 1;
  }
  if (assert_true(resource.resource_report.top_level_type_count == 0,
                  "minimal resource table should decode as empty") != 0) {
    return 1;
  }
  return assert_true(resource.has_resources, "resource flag should be true");
}

int test_parse_invalid_dos_signature() {
  auto bytes = make_minimal_valid_pe_fixture();
  bytes[0] = 'N';
  bytes[1] = 'O';

  const auto parsed = romulus::data::parse_pe_exe_resource(bytes);
  if (assert_true(!parsed.ok(), "invalid DOS signature should fail") != 0) {
    return 1;
  }
  return assert_true(parsed.error.has_value() && parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                     "invalid DOS signature should map to invalid format");
}

int test_import_extraction_for_dll() {
  const auto parsed = romulus::data::parse_pe_exe_resource(make_minimal_valid_pe_fixture());
  if (assert_true(parsed.ok(), "import extraction test requires valid PE") != 0) {
    return 1;
  }

  const auto& imports = parsed.value->imports;
  if (assert_true(imports.size() == 1, "fixture should contain one import descriptor") != 0) {
    return 1;
  }
  return assert_true(imports[0].dll_name == "KERNEL32.dll", "descriptor DLL should decode");
}

int test_report_output_is_stable() {
  romulus::data::PeResourceSectionReport report;
  report.has_resources = true;
  report.resource_rva = 0x3000;
  report.resource_size = 0x200;
  report.top_level_type_count = 1;
  report.leaf_count = 1;
  report.per_type_summary.push_back({.type_id = 3,
                                     .type_label = "ICON",
                                     .type_uses_string_name = false,
                                     .type_string_name = "",
                                     .entry_count = 1,
                                     .leaf_count = 1});
  report.tree.leaves.push_back({.type_id = 3,
                                .type_label = "ICON",
                                .type_uses_string_name = false,
                                .type_string_name = "",
                                .name_id = 10,
                                .name_uses_string_name = false,
                                .name_string = "",
                                .language_id = 1033,
                                .data_rva = 0x3010,
                                .data_size = 12,
                                .data_file_offset = 0x410});

  const auto text = romulus::data::format_pe_resource_report(report);
  if (assert_true(text.find("# Caesar II Win95 PE Resource Report") != std::string::npos,
                  "resource report should include stable title") != 0) {
    return 1;
  }
  if (assert_true(text.find("type: id:3 (ICON)") != std::string::npos,
                  "resource report should include known type label") != 0) {
    return 1;
  }
  return assert_true(text.find("language_id: 1033") != std::string::npos,
                     "resource report should include language entry");
}

int test_report_output_for_string_named_entries_is_stable() {
  romulus::data::PeResourceSectionReport report;
  report.has_resources = true;
  report.resource_rva = 0x3000;
  report.resource_size = 0x200;
  report.top_level_type_count = 1;
  report.leaf_count = 1;
  report.per_type_summary.push_back({.type_id = 0,
                                     .type_label = "",
                                     .type_uses_string_name = true,
                                     .type_string_name = "NAME",
                                     .entry_count = 1,
                                     .leaf_count = 1});
  report.tree.leaves.push_back({.type_id = 0,
                                .type_label = "",
                                .type_uses_string_name = true,
                                .type_string_name = "NAME",
                                .name_id = 0,
                                .name_uses_string_name = true,
                                .name_string = "SRC",
                                .language_id = 0,
                                .data_rva = 0x3010,
                                .data_size = 12,
                                .data_file_offset = 0x410});

  const auto text = romulus::data::format_pe_resource_report(report);
  if (assert_true(text.find("type: name:NAME") != std::string::npos,
                  "resource report should format string-named type") != 0) {
    return 1;
  }
  return assert_true(text.find("name: name:SRC") != std::string::npos,
                     "resource report should format string-named entry");
}

}  // namespace

int main() {
  if (test_parse_valid_minimal_pe() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_invalid_dos_signature() != 0) {
    return EXIT_FAILURE;
  }
  if (test_import_extraction_for_dll() != 0) {
    return EXIT_FAILURE;
  }
  if (test_report_output_is_stable() != 0) {
    return EXIT_FAILURE;
  }
  if (test_report_output_for_string_named_entries_is_stable() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
