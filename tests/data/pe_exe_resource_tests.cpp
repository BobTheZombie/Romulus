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

  // PE signature
  bytes[0x80] = 'P';
  bytes[0x81] = 'E';

  // IMAGE_FILE_HEADER
  set_u16_le(bytes, 0x84, 0x014C);
  set_u16_le(bytes, 0x86, 2);
  set_u32_le(bytes, 0x88, 0x5F3759DF);
  set_u16_le(bytes, 0x94, 224);
  set_u16_le(bytes, 0x96, 0x0102);

  // IMAGE_OPTIONAL_HEADER32
  const std::size_t opt = 0x98;
  set_u16_le(bytes, opt + 0, 0x10B);
  set_u32_le(bytes, opt + 16, 0x1010);  // AddressOfEntryPoint
  set_u32_le(bytes, opt + 28, 0x400000);
  set_u16_le(bytes, opt + 68, 2);       // Subsystem
  set_u32_le(bytes, opt + 92, 16);      // NumberOfRvaAndSizes

  // Data directories
  set_u32_le(bytes, opt + 96 + (1 * 8) + 0, 0x2000);  // Import RVA
  set_u32_le(bytes, opt + 96 + (1 * 8) + 4, 0x40);    // Import size
  set_u32_le(bytes, opt + 96 + (2 * 8) + 0, 0x2090);  // Resource RVA
  set_u32_le(bytes, opt + 96 + (2 * 8) + 4, 0x10);    // Resource size

  // Section headers at 0x178
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

  // Import descriptor @ raw 0x400
  set_u32_le(bytes, 0x400, 0x2040);  // OriginalFirstThunk
  set_u32_le(bytes, 0x40C, 0x2060);  // Name RVA

  // Thunk table @ raw 0x440
  set_u32_le(bytes, 0x440, 0x2070);
  set_u32_le(bytes, 0x444, 0x80001234);  // ordinal only
  set_u32_le(bytes, 0x448, 0x2080);

  // Strings
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
  if (assert_true(resource.entry_point_rva == 0x1010, "entry point RVA should decode") != 0) {
    return 1;
  }
  if (assert_true(resource.subsystem == 2, "subsystem should decode") != 0) {
    return 1;
  }
  if (assert_true(resource.section_count == 2, "section count should decode") != 0) {
    return 1;
  }
  if (assert_true(resource.timestamp == 0x5F3759DF, "timestamp should decode") != 0) {
    return 1;
  }
  if (assert_true(resource.has_resources, "resource flag should be true") != 0) {
    return 1;
  }
  return assert_true(!resource.has_relocations, "relocation flag should be false");
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

int test_parse_invalid_pe_signature() {
  auto bytes = make_minimal_valid_pe_fixture();
  bytes[0x80] = 'X';

  const auto parsed = romulus::data::parse_pe_exe_resource(bytes);
  if (assert_true(!parsed.ok(), "invalid PE signature should fail") != 0) {
    return 1;
  }
  return assert_true(parsed.error.has_value() && parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                     "invalid PE signature should map to invalid format");
}

int test_parse_rejects_truncated_import_directory() {
  auto bytes = make_minimal_valid_pe_fixture();
  bytes.resize(0x450);

  const auto parsed = romulus::data::parse_pe_exe_resource(bytes);
  if (assert_true(!parsed.ok(), "truncated import directory should fail") != 0) {
    return 1;
  }
  return assert_true(parsed.error.has_value(), "truncated import directory should provide structured error");
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
  if (assert_true(imports[0].dll_name == "KERNEL32.dll", "descriptor DLL should decode") != 0) {
    return 1;
  }
  if (assert_true(imports[0].imported_functions.size() == 2,
                  "ordinal-only import should be skipped from named function list") != 0) {
    return 1;
  }
  return assert_true(imports[0].imported_functions[0] == "CreateFileA" &&
                         imports[0].imported_functions[1] == "ReadFile",
                     "named imports should decode in thunk order");
}

int test_report_output_is_stable() {
  const auto parsed = romulus::data::parse_pe_exe_resource(make_minimal_valid_pe_fixture());
  if (assert_true(parsed.ok(), "report test requires valid PE") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_pe_exe_report(parsed.value.value());
  if (assert_true(report.find("# Caesar II Win95 PE EXE Report") != std::string::npos,
                  "report should include stable title") != 0) {
    return 1;
  }
  if (assert_true(report.find("entry_point_rva: 0x00001010") != std::string::npos,
                  "report should include deterministic entry point") != 0) {
    return 1;
  }
  if (assert_true(report.find("KERNEL32.dll!CreateFileA") != std::string::npos,
                  "report should include categorized file I/O summary entry") != 0) {
    return 1;
  }
  return assert_true(report.find("import_descriptor_count: 1") != std::string::npos,
                     "report should include descriptor count");
}

}  // namespace

int main() {
  if (test_parse_valid_minimal_pe() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_invalid_dos_signature() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_invalid_pe_signature() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_rejects_truncated_import_directory() != 0) {
    return EXIT_FAILURE;
  }
  if (test_import_extraction_for_dll() != 0) {
    return EXIT_FAILURE;
  }
  if (test_report_output_is_stable() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
