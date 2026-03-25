#include "romulus/data/win95_data_container.h"

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

void append_u32_le(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

std::vector<std::uint8_t> make_valid_pack_fixture() {
  std::vector<std::uint8_t> bytes;
  bytes.push_back('P');
  bytes.push_back('A');
  bytes.push_back('C');
  bytes.push_back('K');
  append_u32_le(bytes, 2);

  append_u32_le(bytes, 24);
  append_u32_le(bytes, 4);
  append_u32_le(bytes, 28);
  append_u32_le(bytes, 4);

  bytes.push_back(0x10);
  bytes.push_back(0x11);
  bytes.push_back(0x12);
  bytes.push_back(0x13);
  bytes.push_back(0x20);
  bytes.push_back(0x21);
  bytes.push_back(0x22);
  bytes.push_back(0x23);
  return bytes;
}

std::vector<std::uint8_t> make_signature_rich_pack_fixture() {
  std::vector<std::uint8_t> bytes;
  bytes.push_back('P');
  bytes.push_back('A');
  bytes.push_back('C');
  bytes.push_back('K');
  append_u32_le(bytes, 4);

  append_u32_le(bytes, 40);
  append_u32_le(bytes, 12);
  append_u32_le(bytes, 52);
  append_u32_le(bytes, 12);
  append_u32_le(bytes, 64);
  append_u32_le(bytes, 8);
  append_u32_le(bytes, 72);
  append_u32_le(bytes, 4);

  bytes.insert(bytes.end(), {'F', 'O', 'R', 'M', 'I', 'L', 'B', 'M', 0x00, 0x01, 0x02, 0x03});
  bytes.insert(bytes.end(), {'N', 'A', 'M', 'E', '=', 'R', 'O', 'M', 'E', '\n', 'X', '\n'});
  bytes.insert(bytes.end(), {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80});
  bytes.insert(bytes.end(), {0xAA, 0xBB, 0xCC, 0xDD});
  return bytes;
}

int test_parse_valid_pack_container() {
  const auto bytes = make_valid_pack_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);

  if (assert_true(parsed.ok(), "valid PACK fixture should parse") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->header.signature == "PACK", "signature should be PACK") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->header.entry_count == 2, "entry count should be decoded") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->entries.size() == 2, "entry table size should match count") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->entries[0].offset == 24 && parsed.value->entries[0].size == 4,
                  "first entry range should decode") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->entries[0].signature_hex == "10 11 12 13",
                  "entry signature hex should be collected") != 0) {
    return 1;
  }

  return 0;
}

int test_rejects_bad_signature() {
  auto bytes = make_valid_pack_fixture();
  bytes[0] = 'B';

  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(!parsed.ok(), "bad signature should fail") != 0) {
    return 1;
  }

  return assert_true(parsed.error.has_value() && parsed.error->offset == 0,
                     "bad signature should report magic offset");
}

int test_rejects_truncated_entry_table() {
  auto bytes = make_valid_pack_fixture();
  bytes.resize(12);

  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(!parsed.ok(), "truncated entry table should fail") != 0) {
    return 1;
  }

  return assert_true(parsed.error.has_value() && parsed.error->offset == 4,
                     "truncated table should fail at count/table validation offset");
}

int test_rejects_invalid_count_or_offset() {
  auto bytes = make_valid_pack_fixture();
  bytes[8] = 0x02;
  bytes[9] = 0x00;
  bytes[10] = 0x00;
  bytes[11] = 0x00;

  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(!parsed.ok(), "entry offset into table should fail") != 0) {
    return 1;
  }

  return assert_true(parsed.error.has_value() && parsed.error->offset == 8,
                     "invalid entry range should report record offset");
}

int test_stable_report_formatting() {
  const auto bytes = make_valid_pack_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "stable report test requires valid parse") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_win95_data_container_report(parsed.value.value(), "DATA");
  if (assert_true(report.find("# Caesar II Win95 PACK Container Report") != std::string::npos,
                  "report should include stable title") != 0) {
    return 1;
  }

  if (assert_true(report.find("source: DATA") != std::string::npos,
                  "report should include source label") != 0) {
    return 1;
  }

  if (assert_true(report.find("entries_preview: showing 2 of 2") != std::string::npos,
                  "report should show bounded preview header") != 0) {
    return 1;
  }

  return assert_true(report.find("index: 1 offset=28 size=4 end_offset=32") != std::string::npos,
                     "report should include deterministic entry lines");
}

int test_reports_summary_and_classification_hints() {
  const auto bytes = make_signature_rich_pack_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "summary test requires valid parse") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->summary.entry_count == 4, "summary entry count should be populated") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->summary.total_payload_bytes == 36, "summary payload bytes should be stable") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->entries[0].classification_hint == "possible-ilbm", "FORM entries should hint ILBM") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->entries[1].classification_hint == "text-like", "printable entries should hint text") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->entries[2].classification_hint == "opaque-binary", "opaque entries should remain opaque") != 0) {
    return 1;
  }

  if (assert_true(parsed.value->summary.recognizable_signature_count == 2, "recognizable signature tally should be bounded") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_win95_data_container_report(
      parsed.value.value(),
      "DATA",
      romulus::data::Win95PackReportOptions{
          .preview_entry_limit = 2,
          .include_all_entries = false,
      });
  if (assert_true(report.find("entries_preview: showing 2 of 4") != std::string::npos,
                  "report should obey preview cap") != 0) {
    return 1;
  }

  if (assert_true(report.find("entries_preview_truncated: yes") != std::string::npos,
                  "report should declare truncation") != 0) {
    return 1;
  }

  return assert_true(report.find("signature_frequency:") != std::string::npos,
                     "report should include signature frequency summary");
}

int test_extended_preview_includes_all_entries() {
  const auto bytes = make_signature_rich_pack_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "extended preview requires valid parse") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_win95_data_container_report(
      parsed.value.value(),
      "DATA",
      romulus::data::Win95PackReportOptions{
          .preview_entry_limit = 1,
          .include_all_entries = true,
      });
  if (assert_true(report.find("entries_preview: showing 4 of 4") != std::string::npos,
                  "extended mode should include all entries") != 0) {
    return 1;
  }

  return assert_true(report.find("entries_preview_truncated: yes") == std::string::npos,
                     "extended mode should not report truncation");
}

int test_rejects_truncated_entry_payload_range() {
  auto bytes = make_valid_pack_fixture();
  bytes[9] = 0x01;

  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(!parsed.ok(), "out-of-bounds entry should fail") != 0) {
    return 1;
  }

  return assert_true(parsed.error.has_value() && parsed.error->offset == 8,
                     "entry payload range failure should report entry record offset");
}

}  // namespace

int main() {
  if (test_parse_valid_pack_container() != 0) {
    return EXIT_FAILURE;
  }

  if (test_rejects_bad_signature() != 0) {
    return EXIT_FAILURE;
  }

  if (test_rejects_truncated_entry_table() != 0) {
    return EXIT_FAILURE;
  }

  if (test_rejects_invalid_count_or_offset() != 0) {
    return EXIT_FAILURE;
  }

  if (test_stable_report_formatting() != 0) {
    return EXIT_FAILURE;
  }

  if (test_reports_summary_and_classification_hints() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extended_preview_includes_all_entries() != 0) {
    return EXIT_FAILURE;
  }

  if (test_rejects_truncated_entry_payload_range() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
