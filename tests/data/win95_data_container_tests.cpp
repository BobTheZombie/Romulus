#include "romulus/data/win95_data_container.h"

#include "romulus/data/ilbm_image.h"

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

void append_u16_be(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
  out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void append_u32_be(std::vector<std::uint8_t>& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
  out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void append_chunk(std::vector<std::uint8_t>& out, const char id[4], const std::vector<std::uint8_t>& payload) {
  out.push_back(static_cast<std::uint8_t>(id[0]));
  out.push_back(static_cast<std::uint8_t>(id[1]));
  out.push_back(static_cast<std::uint8_t>(id[2]));
  out.push_back(static_cast<std::uint8_t>(id[3]));
  append_u32_be(out, static_cast<std::uint32_t>(payload.size()));
  out.insert(out.end(), payload.begin(), payload.end());
  if ((payload.size() & 1U) != 0U) {
    out.push_back(0x00);
  }
}

std::vector<std::uint8_t> make_fixture_ilbm_payload() {
  std::vector<std::uint8_t> bmhd;
  append_u16_be(bmhd, 2);
  append_u16_be(bmhd, 2);
  append_u16_be(bmhd, 0);
  append_u16_be(bmhd, 0);
  bmhd.push_back(8);
  bmhd.push_back(0);
  bmhd.push_back(0);
  bmhd.push_back(0);
  append_u16_be(bmhd, 0);
  bmhd.push_back(10);
  bmhd.push_back(11);
  append_u16_be(bmhd, 2);
  append_u16_be(bmhd, 2);

  const std::vector<std::uint8_t> cmap = {
      0, 0, 0,
      20, 30, 40,
      40, 50, 60,
      70, 80, 90,
      100, 110, 120,
  };

  const std::vector<std::uint8_t> body = {
      0x80, 0x00,
      0x40, 0x00,
      0x00, 0x00,
      0x00, 0x00,
      0x00, 0x00,
      0x00, 0x00,
      0x00, 0x00,
      0x00, 0x00,
      0x80, 0x00,
      0x80, 0x00,
      0x40, 0x00,
      0x00, 0x00,
      0x00, 0x00,
      0x00, 0x00,
      0x00, 0x00,
      0x00, 0x00,
  };

  std::vector<std::uint8_t> form_payload;
  form_payload.insert(form_payload.end(), {'I', 'L', 'B', 'M'});
  append_chunk(form_payload, "BMHD", bmhd);
  append_chunk(form_payload, "CMAP", cmap);
  append_chunk(form_payload, "BODY", body);

  std::vector<std::uint8_t> bytes = {'F', 'O', 'R', 'M'};
  append_u32_be(bytes, static_cast<std::uint32_t>(form_payload.size()));
  bytes.insert(bytes.end(), form_payload.begin(), form_payload.end());
  return bytes;
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

std::vector<std::uint8_t> make_pack_with_ilbm_fixture() {
  const auto ilbm_payload = make_fixture_ilbm_payload();
  const std::uint32_t entry_table_offset = 8;
  const std::uint32_t entry_record_size = 8;
  const std::uint32_t entry_count = 2;
  const std::uint32_t payload0_offset = entry_table_offset + (entry_record_size * entry_count);
  const std::uint32_t payload1_offset = payload0_offset + static_cast<std::uint32_t>(ilbm_payload.size());

  std::vector<std::uint8_t> bytes;
  bytes.insert(bytes.end(), {'P', 'A', 'C', 'K'});
  append_u32_le(bytes, entry_count);
  append_u32_le(bytes, payload0_offset);
  append_u32_le(bytes, static_cast<std::uint32_t>(ilbm_payload.size()));
  append_u32_le(bytes, payload1_offset);
  append_u32_le(bytes, 4);
  bytes.insert(bytes.end(), ilbm_payload.begin(), ilbm_payload.end());
  bytes.insert(bytes.end(), {0xDE, 0xAD, 0xBE, 0xEF});
  return bytes;
}

std::vector<std::uint8_t> make_pack_with_malformed_ilbm_fixture() {
  const std::vector<std::uint8_t> malformed_payload = {'F', 'O', 'R', 'M', 0x00, 0x00, 0x00, 0x04, 'B', 'A', 'D', '!'};
  std::vector<std::uint8_t> bytes;
  bytes.insert(bytes.end(), {'P', 'A', 'C', 'K'});
  append_u32_le(bytes, 1);
  append_u32_le(bytes, 16);
  append_u32_le(bytes, static_cast<std::uint32_t>(malformed_payload.size()));
  bytes.insert(bytes.end(), malformed_payload.begin(), malformed_payload.end());
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

int test_extract_pack_ilbm_entry_success() {
  const auto bytes = make_pack_with_ilbm_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "PACK fixture with ILBM should parse") != 0) {
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_ilbm_entry(bytes, parsed.value.value(), 0);
  if (assert_true(extracted.ok(), "ILBM-like entry should extract and parse") != 0) {
    return 1;
  }

  if (assert_true(extracted.value->ilbm.width == 2 && extracted.value->ilbm.height == 2,
                  "extracted ILBM should decode known dimensions") != 0) {
    return 1;
  }

  const auto rgba = romulus::data::convert_ilbm_to_rgba(extracted.value->ilbm);
  if (assert_true(rgba.ok(), "extracted ILBM should reuse RGBA conversion path") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_lbm_report(extracted.value->ilbm, 2);
  return assert_true(report.find("# Caesar II Win95 LBM Report") != std::string::npos,
                     "extracted ILBM should reuse stable LBM report formatting");
}

int test_extract_pack_ilbm_entry_rejects_invalid_index() {
  const auto bytes = make_pack_with_ilbm_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "invalid index test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_ilbm_entry(bytes, parsed.value.value(), 3);
  if (assert_true(!extracted.ok(), "invalid entry index should fail") != 0) {
    return 1;
  }

  return assert_true(extracted.error->message.find("Invalid PACK entry index") != std::string::npos,
                     "invalid index error should be explicit");
}

int test_extract_pack_ilbm_entry_rejects_non_ilbm_classification() {
  const auto bytes = make_pack_with_ilbm_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "non-ilbm classification test requires valid parse") != 0) {
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_ilbm_entry(bytes, parsed.value.value(), 1);
  if (assert_true(!extracted.ok(), "non-ILBM class entries should remain unsupported") != 0) {
    return 1;
  }

  return assert_true(extracted.error->message.find("unsupported for extraction") != std::string::npos,
                     "non-ILBM classification error should be explicit");
}

int test_extract_pack_ilbm_entry_rejects_truncated_payload_bounds() {
  const auto bytes = make_pack_with_ilbm_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "truncated payload test requires valid PACK parse") != 0) {
    return 1;
  }

  std::vector<std::uint8_t> truncated = bytes;
  truncated.resize(40);
  const auto extracted = romulus::data::extract_win95_pack_ilbm_entry(truncated, parsed.value.value(), 0);
  if (assert_true(!extracted.ok(), "truncated payload bounds should fail extraction") != 0) {
    return 1;
  }

  return assert_true(extracted.error->message.find("exceeds container bounds") != std::string::npos,
                     "truncated payload should report bounded extraction failure");
}

int test_extract_pack_ilbm_entry_rejects_malformed_ilbm_payload() {
  const auto bytes = make_pack_with_malformed_ilbm_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "malformed ILBM test requires PACK parse") != 0) {
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_ilbm_entry(bytes, parsed.value.value(), 0);
  if (assert_true(!extracted.ok(), "malformed ILBM payload should fail") != 0) {
    return 1;
  }

  return assert_true(extracted.error->message.find("failed ILBM validation") != std::string::npos,
                     "malformed ILBM should return explicit validation failure");
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

  if (test_extract_pack_ilbm_entry_success() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_ilbm_entry_rejects_invalid_index() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_ilbm_entry_rejects_non_ilbm_classification() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_ilbm_entry_rejects_truncated_payload_bounds() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_ilbm_entry_rejects_malformed_ilbm_payload() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
