#include "romulus/data/pl8_resource.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

std::vector<std::uint8_t> make_supported_pl8_fixture() {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(romulus::data::Pl8Resource::kSupportedPayloadSize);

  for (std::size_t index = 0; index < romulus::data::Pl8Resource::kSupportedEntryCount; ++index) {
    bytes.push_back(static_cast<std::uint8_t>(index));
    bytes.push_back(static_cast<std::uint8_t>((index * 2U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((255U - index) & 0xFFU));
  }

  return bytes;
}

std::filesystem::path make_temp_test_dir() {
  const auto unique = std::to_string(std::rand()) + "_" + std::to_string(std::rand());
  return std::filesystem::temp_directory_path() / ("romulus_pl8_resource_tests_" + unique);
}

int test_parse_pl8_success() {
  const auto bytes = make_supported_pl8_fixture();
  const auto parsed = romulus::data::parse_pl8_resource(bytes);

  if (assert_true(parsed.ok(), "supported PL8 fixture should parse") != 0) {
    return 1;
  }

  const auto& resource = parsed.value.value();
  if (assert_true(resource.payload_size == romulus::data::Pl8Resource::kSupportedPayloadSize,
                  "payload_size should match fixed supported size") != 0) {
    return 1;
  }

  if (assert_true(resource.palette_entries.size() == romulus::data::Pl8Resource::kSupportedEntryCount,
                  "palette should decode 256 entries") != 0) {
    return 1;
  }

  if (assert_true(resource.palette_entries[0].red == 0 && resource.palette_entries[0].green == 0 &&
                      resource.palette_entries[0].blue == 255,
                  "first entry should match fixture") != 0) {
    return 1;
  }

  return assert_true(resource.palette_entries[255].red == 255 && resource.palette_entries[255].green == 254 &&
                         resource.palette_entries[255].blue == 0,
                     "last entry should match fixture");
}

int test_parse_pl8_rejects_unsupported_layout_size() {
  auto bytes = make_supported_pl8_fixture();
  bytes.pop_back();

  const auto parsed = romulus::data::parse_pl8_resource(bytes);
  if (assert_true(!parsed.ok(), "unsupported size should fail") != 0) {
    return 1;
  }

  if (assert_true(parsed.error.has_value() && parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                  "unsupported size should map to invalid format") != 0) {
    return 1;
  }

  return assert_true(parsed.error.has_value() && parsed.error->offset == 0,
                     "unsupported size should report offset 0");
}

int test_format_pl8_report_is_stable() {
  const auto bytes = make_supported_pl8_fixture();
  const auto parsed = romulus::data::parse_pl8_resource(bytes);
  if (assert_true(parsed.ok(), "supported fixture should parse before report formatting") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_pl8_report(parsed.value.value(), 2);

  if (assert_true(report.find("# Caesar II Win95 PL8 Report") != std::string::npos,
                  "report should include title") != 0) {
    return 1;
  }

  if (assert_true(report.find("supported_layout: raw_rgb_triplets_256") != std::string::npos,
                  "report should include supported layout label") != 0) {
    return 1;
  }

  if (assert_true(report.find("palette_preview_count: 2") != std::string::npos,
                  "report should include requested preview count") != 0) {
    return 1;
  }

  return assert_true(report.find("index: 1 rgb=(1,2,254) hex=(0x01,0x02,0xfe)") != std::string::npos,
                     "report should include deterministic preview entry");
}

int test_batch_probe_reports_supported_and_unsupported_layouts() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir);

  {
    std::ofstream supported(temp_dir / "GOOD.PL8", std::ios::binary);
    const auto bytes = make_supported_pl8_fixture();
    supported.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }

  {
    std::ofstream unsupported(temp_dir / "ODD.PL8", std::ios::binary);
    unsupported.write("12345", 5);
  }

  const auto result = romulus::data::probe_pl8_files(temp_dir, {"GOOD.PL8", "ODD.PL8"});
  if (assert_true(result.ok(), "batch probe should succeed for supported and unsupported layouts") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto& files = result.value->files;
  if (assert_true(files.size() == 2, "batch probe should report two files") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(files[0].matches_supported_palette_layout, "768-byte file should be marked supported") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(!files[1].matches_supported_palette_layout, "non-768-byte file should be marked unsupported") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto formatted = romulus::data::format_pl8_batch_report(result.value.value(), 2);
  if (assert_true(formatted.find("path: GOOD.PL8") != std::string::npos, "report should include first path") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(formatted.find("palette_summary: palette_entries=256 preview=[0:(0,0,255), 1:(1,2,254)]") !=
                      std::string::npos,
                  "supported file should include deterministic compact palette preview") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(
          formatted.find("unsupported_summary: unsupported_layout: payload_size=5 expected_payload_size=768") !=
              std::string::npos,
          "unsupported file should include deterministic unsupported summary") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

}  // namespace

int main() {
  std::srand(1816);

  if (test_parse_pl8_success() != 0) {
    return 1;
  }

  if (test_parse_pl8_rejects_unsupported_layout_size() != 0) {
    return 1;
  }

  if (test_format_pl8_report_is_stable() != 0) {
    return 1;
  }

  if (test_batch_probe_reports_supported_and_unsupported_layouts() != 0) {
    return 1;
  }

  return 0;
}
