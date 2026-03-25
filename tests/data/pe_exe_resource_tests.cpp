#include "romulus/data/pe_exe_resource.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
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

void append_u16_le(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32_le(std::vector<std::uint8_t>& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void append_utf16_z(std::vector<std::uint8_t>& out, std::string_view text) {
  for (const char ch : text) {
    append_u16_le(out, static_cast<std::uint16_t>(static_cast<unsigned char>(ch)));
  }
  append_u16_le(out, 0);
}

void align_append_4(std::vector<std::uint8_t>& out) {
  while ((out.size() % 4) != 0) {
    out.push_back(0);
  }
}

std::vector<std::uint8_t> make_version_resource_payload() {
  std::vector<std::uint8_t> payload;
  append_u16_le(payload, 0);  // root length placeholder
  append_u16_le(payload, 52 / 2);  // root value length in WORDs
  append_u16_le(payload, 0);       // binary
  append_utf16_z(payload, "VS_VERSION_INFO");
  align_append_4(payload);

  append_u32_le(payload, 0xFEEF04BD);
  append_u32_le(payload, 0x00010000);
  append_u32_le(payload, 0x00020003);
  append_u32_le(payload, 0x00040005);
  append_u32_le(payload, 0x00060007);
  for (int i = 0; i < 8; ++i) {
    append_u32_le(payload, 0);
  }
  align_append_4(payload);

  const std::size_t child_offset = payload.size();
  append_u16_le(payload, 0);  // child length placeholder
  append_u16_le(payload, 4);  // "Game"
  append_u16_le(payload, 1);  // text
  append_utf16_z(payload, "FileDescription");
  align_append_4(payload);
  append_u16_le(payload, 'G');
  append_u16_le(payload, 'a');
  append_u16_le(payload, 'm');
  append_u16_le(payload, 'e');
  align_append_4(payload);

  const std::uint16_t child_length = static_cast<std::uint16_t>(payload.size() - child_offset);
  payload[child_offset + 0] = static_cast<std::uint8_t>(child_length & 0xFFU);
  payload[child_offset + 1] = static_cast<std::uint8_t>((child_length >> 8U) & 0xFFU);

  const std::uint16_t root_length = static_cast<std::uint16_t>(payload.size());
  payload[0] = static_cast<std::uint8_t>(root_length & 0xFFU);
  payload[1] = static_cast<std::uint8_t>((root_length >> 8U) & 0xFFU);
  return payload;
}

std::vector<std::uint8_t> make_string_table_payload() {
  std::vector<std::uint8_t> payload;
  payload.reserve(128);
  for (int i = 0; i < 16; ++i) {
    if (i == 0) {
      append_u16_le(payload, 5);
      append_u16_le(payload, 'H');
      append_u16_le(payload, 'e');
      append_u16_le(payload, 'l');
      append_u16_le(payload, 'l');
      append_u16_le(payload, 'o');
    } else {
      append_u16_le(payload, 0);
    }
  }
  return payload;
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

int test_decode_version_resource_payload() {
  const auto payload = make_version_resource_payload();
  std::vector<std::uint8_t> bytes(128, 0);
  const std::size_t offset = bytes.size();
  bytes.insert(bytes.end(), payload.begin(), payload.end());

  romulus::data::PeExeResource resource;
  resource.resource_report.tree.leaves.push_back({.type_id = 16,
                                                  .type_label = "VERSION",
                                                  .name_id = 1,
                                                  .language_id = 1033,
                                                  .data_size = static_cast<std::uint32_t>(payload.size()),
                                                  .data_file_offset = offset});

  const auto decoded = romulus::data::decode_pe_resource_payloads(bytes, resource);
  if (assert_true(decoded.ok(), "valid VERSION payload should decode") != 0) {
    return 1;
  }
  if (assert_true(decoded.value->version_resources.size() == 1, "one VERSION payload should decode") != 0) {
    return 1;
  }
  const auto& version = decoded.value->version_resources.front();
  if (assert_true(version.has_fixed_file_info, "fixed file info should decode") != 0) {
    return 1;
  }
  if (assert_true(version.file_version_ms == 0x00020003, "file version should decode") != 0) {
    return 1;
  }
  return assert_true(!version.string_values.empty() && version.string_values.front().key == "FileDescription",
                     "VERSION string key should decode");
}

int test_decode_malformed_version_payload_fails() {
  auto payload = make_version_resource_payload();
  payload[0] = 0xFF;
  payload[1] = 0x7F;
  std::vector<std::uint8_t> bytes(64, 0);
  const std::size_t offset = bytes.size();
  bytes.insert(bytes.end(), payload.begin(), payload.end());

  romulus::data::PeExeResource resource;
  resource.resource_report.tree.leaves.push_back({.type_id = 16,
                                                  .type_label = "VERSION",
                                                  .name_id = 1,
                                                  .language_id = 1033,
                                                  .data_size = static_cast<std::uint32_t>(payload.size()),
                                                  .data_file_offset = offset});
  const auto decoded = romulus::data::decode_pe_resource_payloads(bytes, resource);
  return assert_true(!decoded.ok(), "malformed VERSION payload should fail");
}

int test_decode_string_table_payload() {
  const auto payload = make_string_table_payload();
  std::vector<std::uint8_t> bytes(32, 0);
  const std::size_t offset = bytes.size();
  bytes.insert(bytes.end(), payload.begin(), payload.end());

  romulus::data::PeExeResource resource;
  resource.resource_report.tree.leaves.push_back({.type_id = 6,
                                                  .type_label = "STRING",
                                                  .name_id = 2,
                                                  .language_id = 1033,
                                                  .data_size = static_cast<std::uint32_t>(payload.size()),
                                                  .data_file_offset = offset});
  const auto decoded = romulus::data::decode_pe_resource_payloads(bytes, resource);
  if (assert_true(decoded.ok(), "valid STRINGTABLE payload should decode") != 0) {
    return 1;
  }
  if (assert_true(decoded.value->string_table_resources.size() == 1, "one string table should decode") != 0) {
    return 1;
  }
  const auto& entries = decoded.value->string_table_resources.front().entries;
  if (assert_true(entries.size() == 1, "single non-empty string should decode") != 0) {
    return 1;
  }
  if (assert_true(entries.front().string_id == 16, "string id should preserve bundle base id") != 0) {
    return 1;
  }
  return assert_true(entries.front().text == "Hello", "string text should decode");
}

int test_decode_malformed_string_table_payload_fails() {
  auto payload = make_string_table_payload();
  payload.resize(payload.size() - 2);
  std::vector<std::uint8_t> bytes(32, 0);
  const std::size_t offset = bytes.size();
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  romulus::data::PeExeResource resource;
  resource.resource_report.tree.leaves.push_back({.type_id = 6,
                                                  .type_label = "STRING",
                                                  .name_id = 2,
                                                  .language_id = 1033,
                                                  .data_size = static_cast<std::uint32_t>(payload.size()),
                                                  .data_file_offset = offset});
  const auto decoded = romulus::data::decode_pe_resource_payloads(bytes, resource);
  return assert_true(!decoded.ok(), "malformed STRINGTABLE payload should fail");
}

int test_payload_report_output_is_stable() {
  romulus::data::PeResourcePayloadReport report;
  report.version_resources.push_back({.source_leaf = {.type_id = 16, .type_label = "VERSION", .name_id = 1, .language_id = 1033},
                                      .has_fixed_file_info = true,
                                      .file_version_ms = 0x00020003,
                                      .file_version_ls = 0x00040005,
                                      .product_version_ms = 0x00060007,
                                      .product_version_ls = 0x00080009,
                                      .string_values = {{"CompanyName", "Impressions"}}});
  report.string_table_resources.push_back({.source_leaf = {.type_id = 6, .type_label = "STRING", .name_id = 2, .language_id = 1033},
                                           .entries = {{16, "Hello"}}});
  report.skipped_resources.push_back({.source_leaf = {.type_id = 5, .type_label = "DIALOG", .name_id = 100, .language_id = 1033},
                                      .reason = "unsupported resource type"});

  const auto text = romulus::data::format_pe_resource_payload_report(report);
  if (assert_true(text.find("version_resource_count: 1") != std::string::npos,
                  "payload report should include version count") != 0) {
    return 1;
  }
  if (assert_true(text.find("string_table_resource_count: 1") != std::string::npos,
                  "payload report should include string table count") != 0) {
    return 1;
  }
  return assert_true(text.find("reason: unsupported resource type") != std::string::npos,
                     "payload report should include skipped reason");
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
  if (test_decode_version_resource_payload() != 0) {
    return EXIT_FAILURE;
  }
  if (test_decode_malformed_version_payload_fails() != 0) {
    return EXIT_FAILURE;
  }
  if (test_decode_string_table_payload() != 0) {
    return EXIT_FAILURE;
  }
  if (test_decode_malformed_string_table_payload_fails() != 0) {
    return EXIT_FAILURE;
  }
  if (test_payload_report_output_is_stable() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
