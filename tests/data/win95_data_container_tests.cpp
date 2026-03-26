#include "romulus/data/win95_data_container.h"

#include "romulus/data/image_export.h"
#include "romulus/data/ilbm_image.h"

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

std::vector<std::uint8_t> make_pack_with_ilbm_batch_fixture() {
  const auto valid_ilbm = make_fixture_ilbm_payload();
  const std::vector<std::uint8_t> malformed_ilbm = {'F', 'O', 'R', 'M', 0x00, 0x00, 0x00, 0x04, 'B', 'A', 'D', '!'};
  const std::vector<std::uint8_t> opaque_payload = {0xDE, 0xAD, 0xBE, 0xEF};
  std::vector<std::uint8_t> bytes;
  bytes.insert(bytes.end(), {'P', 'A', 'C', 'K'});
  append_u32_le(bytes, 3);

  const std::uint32_t table_end = 8 + (3 * 8);
  const std::uint32_t entry0_offset = table_end;
  const std::uint32_t entry1_offset = entry0_offset + static_cast<std::uint32_t>(valid_ilbm.size());
  const std::uint32_t entry2_offset = entry1_offset + static_cast<std::uint32_t>(malformed_ilbm.size());

  append_u32_le(bytes, entry0_offset);
  append_u32_le(bytes, static_cast<std::uint32_t>(valid_ilbm.size()));
  append_u32_le(bytes, entry1_offset);
  append_u32_le(bytes, static_cast<std::uint32_t>(malformed_ilbm.size()));
  append_u32_le(bytes, entry2_offset);
  append_u32_le(bytes, static_cast<std::uint32_t>(opaque_payload.size()));

  bytes.insert(bytes.end(), valid_ilbm.begin(), valid_ilbm.end());
  bytes.insert(bytes.end(), malformed_ilbm.begin(), malformed_ilbm.end());
  bytes.insert(bytes.end(), opaque_payload.begin(), opaque_payload.end());
  return bytes;
}

std::vector<std::uint8_t> make_pack_with_multiple_successful_ilbm_entries_fixture() {
  const auto valid_a = make_fixture_ilbm_payload();
  auto valid_b = make_fixture_ilbm_payload();
  if (!valid_b.empty()) {
    valid_b.back() = 0x7F;
  }
  const std::vector<std::uint8_t> malformed_ilbm = {'F', 'O', 'R', 'M', 0x00, 0x00, 0x00, 0x04, 'B', 'A', 'D', '!'};

  std::vector<std::uint8_t> bytes;
  bytes.insert(bytes.end(), {'P', 'A', 'C', 'K'});
  append_u32_le(bytes, 3);

  const std::uint32_t table_end = 8 + (3 * 8);
  const std::uint32_t entry0_offset = table_end;
  const std::uint32_t entry1_offset = entry0_offset + static_cast<std::uint32_t>(valid_a.size());
  const std::uint32_t entry2_offset = entry1_offset + static_cast<std::uint32_t>(malformed_ilbm.size());

  append_u32_le(bytes, entry0_offset);
  append_u32_le(bytes, static_cast<std::uint32_t>(valid_a.size()));
  append_u32_le(bytes, entry1_offset);
  append_u32_le(bytes, static_cast<std::uint32_t>(malformed_ilbm.size()));
  append_u32_le(bytes, entry2_offset);
  append_u32_le(bytes, static_cast<std::uint32_t>(valid_b.size()));

  bytes.insert(bytes.end(), valid_a.begin(), valid_a.end());
  bytes.insert(bytes.end(), malformed_ilbm.begin(), malformed_ilbm.end());
  bytes.insert(bytes.end(), valid_b.begin(), valid_b.end());
  return bytes;
}

std::vector<std::uint8_t> make_pack_with_text_like_fixture() {
  const std::vector<std::uint8_t> text_payload = {
      'R', 'O', 'M', 'U', 'L', 'U', 'S', '\n',
      'P', 'A', 'C', 'K', '\t', 'T', 'E', 'X', 'T', '\n'};
  const std::vector<std::uint8_t> binary_payload = {0xFF, 0x00, 0xAA, 0x55};

  std::vector<std::uint8_t> bytes;
  bytes.insert(bytes.end(), {'P', 'A', 'C', 'K'});
  append_u32_le(bytes, 2);

  const std::uint32_t table_end = 8 + (2 * 8);
  const std::uint32_t entry0_offset = table_end;
  const std::uint32_t entry1_offset = entry0_offset + static_cast<std::uint32_t>(text_payload.size());
  append_u32_le(bytes, entry0_offset);
  append_u32_le(bytes, static_cast<std::uint32_t>(text_payload.size()));
  append_u32_le(bytes, entry1_offset);
  append_u32_le(bytes, static_cast<std::uint32_t>(binary_payload.size()));
  bytes.insert(bytes.end(), text_payload.begin(), text_payload.end());
  bytes.insert(bytes.end(), binary_payload.begin(), binary_payload.end());
  return bytes;
}

std::vector<std::uint8_t> make_pack_with_text_batch_fixture() {
  const std::vector<std::uint8_t> text_ok_a = {'A', 'l', 'p', 'h', 'a', '\n', '1', '\n'};
  const std::vector<std::uint8_t> text_bad = {'B', 'A', 'D', '!', '\n', 0x1B, 'L', 'I', 'N', 'E'};
  const std::vector<std::uint8_t> opaque = {0xFF, 0x00, 0x10, 0x20};
  const std::vector<std::uint8_t> text_ok_b = {'B', 'e', 't', 'a', '\n', '2', '\n'};

  std::vector<std::uint8_t> bytes;
  bytes.insert(bytes.end(), {'P', 'A', 'C', 'K'});
  append_u32_le(bytes, 4);
  const std::uint32_t table_end = 8 + (4 * 8);
  const std::uint32_t entry0 = table_end;
  const std::uint32_t entry1 = entry0 + static_cast<std::uint32_t>(text_ok_a.size());
  const std::uint32_t entry2 = entry1 + static_cast<std::uint32_t>(text_bad.size());
  const std::uint32_t entry3 = entry2 + static_cast<std::uint32_t>(opaque.size());

  append_u32_le(bytes, entry0);
  append_u32_le(bytes, static_cast<std::uint32_t>(text_ok_a.size()));
  append_u32_le(bytes, entry1);
  append_u32_le(bytes, static_cast<std::uint32_t>(text_bad.size()));
  append_u32_le(bytes, entry2);
  append_u32_le(bytes, static_cast<std::uint32_t>(opaque.size()));
  append_u32_le(bytes, entry3);
  append_u32_le(bytes, static_cast<std::uint32_t>(text_ok_b.size()));

  bytes.insert(bytes.end(), text_ok_a.begin(), text_ok_a.end());
  bytes.insert(bytes.end(), text_bad.begin(), text_bad.end());
  bytes.insert(bytes.end(), opaque.begin(), opaque.end());
  bytes.insert(bytes.end(), text_ok_b.begin(), text_ok_b.end());
  return bytes;
}

std::vector<std::uint8_t> make_pl8_payload_fixture() {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(romulus::data::Pl8Resource::kSupportedPayloadSize);
  for (std::size_t index = 0; index < romulus::data::Pl8Resource::kSupportedEntryCount; ++index) {
    bytes.push_back(static_cast<std::uint8_t>(index & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((index * 3U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((255U - index) & 0xFFU));
  }
  return bytes;
}

std::vector<std::uint8_t> make_pack_with_pl8_fixture() {
  const auto pl8_payload = make_pl8_payload_fixture();
  const std::vector<std::uint8_t> opaque_payload = {0x12, 0x34, 0x56, 0x78};
  std::vector<std::uint8_t> bytes;
  bytes.insert(bytes.end(), {'P', 'A', 'C', 'K'});
  append_u32_le(bytes, 2);
  const std::uint32_t table_end = 8 + (2 * 8);
  const std::uint32_t entry0_offset = table_end;
  const std::uint32_t entry1_offset = entry0_offset + static_cast<std::uint32_t>(pl8_payload.size());
  append_u32_le(bytes, entry0_offset);
  append_u32_le(bytes, static_cast<std::uint32_t>(pl8_payload.size()));
  append_u32_le(bytes, entry1_offset);
  append_u32_le(bytes, static_cast<std::uint32_t>(opaque_payload.size()));
  bytes.insert(bytes.end(), pl8_payload.begin(), pl8_payload.end());
  bytes.insert(bytes.end(), opaque_payload.begin(), opaque_payload.end());
  return bytes;
}

std::vector<std::uint8_t> make_pack_with_mixed_known_families_fixture() {
  const auto ilbm_payload = make_fixture_ilbm_payload();
  const std::vector<std::uint8_t> text_payload = {'A', 'L', 'P', 'H', 'A', '\n', 'B', 'E', 'T', 'A', '\n'};
  const auto pl8_payload = make_pl8_payload_fixture();
  const std::vector<std::uint8_t> unknown_payload = {0x00, 0x01, 0x02, 0x03, 0x04};

  std::vector<std::uint8_t> bytes;
  bytes.insert(bytes.end(), {'P', 'A', 'C', 'K'});
  append_u32_le(bytes, 4);
  const std::uint32_t table_end = 8 + (4 * 8);
  const std::uint32_t entry0 = table_end;
  const std::uint32_t entry1 = entry0 + static_cast<std::uint32_t>(ilbm_payload.size());
  const std::uint32_t entry2 = entry1 + static_cast<std::uint32_t>(text_payload.size());
  const std::uint32_t entry3 = entry2 + static_cast<std::uint32_t>(pl8_payload.size());

  append_u32_le(bytes, entry0);
  append_u32_le(bytes, static_cast<std::uint32_t>(ilbm_payload.size()));
  append_u32_le(bytes, entry1);
  append_u32_le(bytes, static_cast<std::uint32_t>(text_payload.size()));
  append_u32_le(bytes, entry2);
  append_u32_le(bytes, static_cast<std::uint32_t>(pl8_payload.size()));
  append_u32_le(bytes, entry3);
  append_u32_le(bytes, static_cast<std::uint32_t>(unknown_payload.size()));

  bytes.insert(bytes.end(), ilbm_payload.begin(), ilbm_payload.end());
  bytes.insert(bytes.end(), text_payload.begin(), text_payload.end());
  bytes.insert(bytes.end(), pl8_payload.begin(), pl8_payload.end());
  bytes.insert(bytes.end(), unknown_payload.begin(), unknown_payload.end());
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

int test_batch_analyzes_only_possible_ilbm_entries() {
  const auto bytes = make_pack_with_ilbm_batch_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "batch analysis test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto batch = romulus::data::analyze_win95_pack_ilbm_batch(bytes, parsed.value.value());
  if (assert_true(batch.total_entry_count == 3, "batch summary should include total entry count") != 0) {
    return 1;
  }
  if (assert_true(batch.candidate_entry_count == 2, "batch should only analyze possible-ilbm entries") != 0) {
    return 1;
  }
  if (assert_true(batch.parsed_entry_count == 1, "batch should count successful ILBM parses") != 0) {
    return 1;
  }
  if (assert_true(batch.failed_entry_count == 1, "batch should count failed ILBM parses") != 0) {
    return 1;
  }
  if (assert_true(batch.entry_results.size() == 2, "batch results should only include candidate entries") != 0) {
    return 1;
  }
  if (assert_true(batch.entry_results[0].entry_index == 0 && batch.entry_results[1].entry_index == 1,
                  "batch result ordering should follow deterministic entry index order") != 0) {
    return 1;
  }
  if (assert_true(batch.entry_results[0].parse_success, "first candidate should parse successfully") != 0) {
    return 1;
  }
  if (assert_true(batch.entry_results[0].width == 2 && batch.entry_results[0].height == 2,
                  "successful parse should include compact ILBM dimensions") != 0) {
    return 1;
  }
  if (assert_true(!batch.entry_results[1].parse_success, "malformed candidate should fail parsing") != 0) {
    return 1;
  }
  if (assert_true(batch.entry_results[1].failure_reason.has_value(), "failed entry should include compact reason") != 0) {
    return 1;
  }
  if (assert_true(batch.failure_reason_frequencies.size() == 1,
                  "failure summary should group deterministic reason frequencies") != 0) {
    return 1;
  }
  return assert_true(batch.failure_reason_frequencies[0].count == 1,
                     "failure summary count should match failed candidate count");
}

int test_batch_report_formatting_and_preview_bounds() {
  const auto bytes = make_pack_with_ilbm_batch_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "batch report test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto batch = romulus::data::analyze_win95_pack_ilbm_batch(bytes, parsed.value.value());
  const auto compact_report = romulus::data::format_win95_pack_ilbm_batch_report(
      batch,
      "DATA",
      romulus::data::Win95PackIlbmBatchReportOptions{
          .preview_entry_limit = 1,
          .include_all_entries = false,
      });
  if (assert_true(compact_report.find("# Caesar II Win95 PACK ILBM Batch Report") != std::string::npos,
                  "batch report should include stable title") != 0) {
    return 1;
  }
  if (assert_true(compact_report.find("possible_ilbm_entries: 2") != std::string::npos,
                  "batch report should include candidate summary") != 0) {
    return 1;
  }
  if (assert_true(compact_report.find("ilbm_entries_preview: showing 1 of 2") != std::string::npos,
                  "batch report should honor preview limit") != 0) {
    return 1;
  }
  if (assert_true(compact_report.find("ilbm_entries_preview_truncated: yes") != std::string::npos,
                  "bounded report should declare truncation when preview is capped") != 0) {
    return 1;
  }

  const auto detailed_report = romulus::data::format_win95_pack_ilbm_batch_report(
      batch,
      "DATA",
      romulus::data::Win95PackIlbmBatchReportOptions{
          .preview_entry_limit = 1,
          .include_all_entries = true,
      });
  if (assert_true(detailed_report.find("ilbm_entries_preview: showing 2 of 2") != std::string::npos,
                  "detailed report should include all candidate entries") != 0) {
    return 1;
  }
  return assert_true(detailed_report.find("ilbm_entries_preview_truncated: yes") == std::string::npos,
                     "detailed report should avoid truncation marker");
}

int test_builds_successful_ilbm_index_and_stable_report() {
  const auto bytes = make_pack_with_multiple_successful_ilbm_entries_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "index test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto batch = romulus::data::analyze_win95_pack_ilbm_batch(bytes, parsed.value.value());
  const auto index = romulus::data::build_win95_pack_ilbm_success_index(batch);
  if (assert_true(index.total_entry_count == 3, "index should preserve total entry count") != 0) {
    return 1;
  }
  if (assert_true(index.successful_entry_count == 2, "index should include only successful ILBM entries") != 0) {
    return 1;
  }
  if (assert_true(index.successful_entries.size() == 2, "index successful entries vector should be bounded") != 0) {
    return 1;
  }
  if (assert_true(index.successful_entries[0].entry_index == 0 && index.successful_entries[1].entry_index == 2,
                  "index ordering should remain deterministic by entry index") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_win95_pack_ilbm_index_report(
      index,
      "DATA",
      romulus::data::Win95PackIlbmIndexReportOptions{
          .preview_entry_limit = 1,
          .include_all_entries = false,
      });
  if (assert_true(report.find("# Caesar II Win95 PACK ILBM Success Index Report") != std::string::npos,
                  "index report should include stable title") != 0) {
    return 1;
  }
  if (assert_true(report.find("successful_ilbm_entries: 2") != std::string::npos,
                  "index report should include successful entry count") != 0) {
    return 1;
  }
  if (assert_true(report.find("successful_entries_preview: showing 1 of 2") != std::string::npos,
                  "index report should honor preview bounds") != 0) {
    return 1;
  }
  return assert_true(report.find("successful_entries_preview_truncated: yes") != std::string::npos,
                     "index report should mark truncation when preview is bounded");
}

int test_index_lookup_and_export_path_behavior() {
  const auto bytes = make_pack_with_multiple_successful_ilbm_entries_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "export path test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto batch = romulus::data::analyze_win95_pack_ilbm_batch(bytes, parsed.value.value());
  const auto index = romulus::data::build_win95_pack_ilbm_success_index(batch);

  const auto invalid_lookup = romulus::data::find_win95_pack_ilbm_index_entry(index, 99);
  if (assert_true(!invalid_lookup.has_value(), "invalid entry index should not be found in success index") != 0) {
    return 1;
  }

  const auto nonsuccess_lookup = romulus::data::find_win95_pack_ilbm_index_entry(index, 1);
  if (assert_true(!nonsuccess_lookup.has_value(), "failed ILBM entries should not be exportable from success index") != 0) {
    return 1;
  }

  const auto success_lookup = romulus::data::find_win95_pack_ilbm_index_entry(index, 2);
  if (assert_true(success_lookup.has_value(), "successful entry should be discoverable for export") != 0) {
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_ilbm_entry(bytes, parsed.value.value(), 2);
  if (assert_true(extracted.ok(), "successful indexed entry should extract cleanly") != 0) {
    return 1;
  }

  const auto rgba = romulus::data::convert_ilbm_to_rgba(extracted.value->ilbm);
  if (assert_true(rgba.ok(), "successful indexed entry should convert to RGBA") != 0) {
    return 1;
  }

  const auto output_path = std::filesystem::temp_directory_path() / "romulus_pack_ilbm_index_export.ppm";
  const auto exported = romulus::data::export_rgba_image_as_ppm(rgba.value.value(), output_path);
  if (assert_true(exported.ok(), "successful indexed entry should export via shared PPM pipeline") != 0) {
    return 1;
  }

  const auto file_size = std::filesystem::file_size(output_path);
  std::filesystem::remove(output_path);
  return assert_true(file_size > 0, "exported PPM output should be non-empty");
}

int test_extract_pack_text_entry_success_and_preview_report() {
  const auto bytes = make_pack_with_text_like_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "text-like extraction test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_text_entry(bytes, parsed.value.value(), 0);
  if (assert_true(extracted.ok(), "text-like entry should extract successfully") != 0) {
    return 1;
  }

  if (assert_true(extracted.value->character_count == extracted.value->entry.size,
                  "character count should match payload byte count for bounded ASCII text") != 0) {
    return 1;
  }

  if (assert_true(extracted.value->line_count == 3, "line count should be deterministic from newline count") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_win95_pack_text_report(
      extracted.value.value(),
      "DATA",
      romulus::data::Win95PackTextReportOptions{
          .preview_character_limit = 8,
      });
  if (assert_true(report.find("# Caesar II Win95 PACK Text Entry Report") != std::string::npos,
                  "text report should include stable title") != 0) {
    return 1;
  }
  if (assert_true(report.find("entry_index: 0") != std::string::npos,
                  "text report should include deterministic entry index") != 0) {
    return 1;
  }
  if (assert_true(report.find("text_preview_truncated: yes") != std::string::npos,
                  "text report should explicitly mark truncated previews") != 0) {
    return 1;
  }
  return assert_true(report.find("line_count: 3") != std::string::npos,
                     "text report should include bounded line count");
}

int test_extract_pack_text_entry_rejects_invalid_index() {
  const auto bytes = make_pack_with_text_like_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "invalid text index test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_text_entry(bytes, parsed.value.value(), 5);
  if (assert_true(!extracted.ok(), "invalid text entry index should fail") != 0) {
    return 1;
  }

  return assert_true(extracted.error->message.find("Invalid PACK entry index") != std::string::npos,
                     "invalid text index error should be explicit");
}

int test_extract_pack_text_entry_rejects_non_text_like_classification() {
  const auto bytes = make_pack_with_text_like_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "classification rejection test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_text_entry(bytes, parsed.value.value(), 1);
  if (assert_true(!extracted.ok(), "non-text-like entries should remain unsupported for text extraction") != 0) {
    return 1;
  }

  return assert_true(extracted.error->message.find("unsupported for text extraction") != std::string::npos,
                     "non-text-like classification rejection should be explicit");
}

int test_extract_pack_text_entry_rejects_truncated_payload_bounds() {
  const auto bytes = make_pack_with_text_like_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "truncated text payload test requires valid PACK parse") != 0) {
    return 1;
  }

  std::vector<std::uint8_t> truncated = bytes;
  truncated.resize(parsed.value->entries[0].end_offset - 1);
  const auto extracted = romulus::data::extract_win95_pack_text_entry(truncated, parsed.value.value(), 0);
  if (assert_true(!extracted.ok(), "truncated text payload bounds should fail extraction") != 0) {
    return 1;
  }

  return assert_true(extracted.error->message.find("exceeds container bounds") != std::string::npos,
                     "truncated text payload should report bounded extraction failure");
}

int test_extract_pack_text_entry_rejects_malformed_non_decodable_payload() {
  auto bytes = make_pack_with_text_like_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "malformed text test requires valid PACK parse") != 0) {
    return 1;
  }

  bytes[parsed.value->entries[0].offset] = 0x1BU;
  const auto extracted = romulus::data::extract_win95_pack_text_entry(bytes, parsed.value.value(), 0);
  if (assert_true(!extracted.ok(), "control-byte text payload should fail bounded text validation") != 0) {
    return 1;
  }

  return assert_true(extracted.error->message.find("failed text validation") != std::string::npos,
                     "malformed text payload failure should be explicit");
}

int test_extract_pack_text_export_path_behavior() {
  const auto bytes = make_pack_with_text_like_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "text export test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_text_entry(bytes, parsed.value.value(), 0);
  if (assert_true(extracted.ok(), "text export test requires successful text extraction") != 0) {
    return 1;
  }

  const auto output_path = std::filesystem::temp_directory_path() / "romulus_pack_text_export.txt";
  std::ofstream output(output_path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (assert_true(output.is_open(), "text export path should open temporary file") != 0) {
    return 1;
  }

  output.write(extracted.value->decoded_text.data(),
               static_cast<std::streamsize>(extracted.value->decoded_text.size()));
  output.close();
  if (assert_true(std::filesystem::exists(output_path), "text export should create output file") != 0) {
    return 1;
  }

  const auto file_size = std::filesystem::file_size(output_path);
  std::filesystem::remove(output_path);
  return assert_true(file_size == extracted.value->decoded_text.size(),
                     "exported text file should preserve decoded text size");
}

int test_batch_analyzes_text_like_entries_and_groups_failures() {
  const auto bytes = make_pack_with_text_batch_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "text batch test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto batch = romulus::data::analyze_win95_pack_text_batch(bytes, parsed.value.value(), 12);
  if (assert_true(batch.total_entry_count == 4, "text batch should report total entries") != 0) {
    return 1;
  }
  if (assert_true(batch.candidate_entry_count == 3, "text batch should analyze only text-like entries") != 0) {
    return 1;
  }
  if (assert_true(batch.decoded_entry_count == 2, "text batch should count successful decodes") != 0) {
    return 1;
  }
  if (assert_true(batch.failed_entry_count == 1, "text batch should count decode failures") != 0) {
    return 1;
  }
  if (assert_true(batch.entry_results.size() == 3, "text batch results should be deterministic per text-like entry") != 0) {
    return 1;
  }
  if (assert_true(batch.entry_results[0].entry_index == 0 && batch.entry_results[1].entry_index == 1 &&
                      batch.entry_results[2].entry_index == 3,
                  "text batch entry ordering should remain entry-index ordered") != 0) {
    return 1;
  }
  if (assert_true(!batch.failure_reason_frequencies.empty() &&
                      batch.failure_reason_frequencies[0].signature.find("unsupported byte") != std::string::npos,
                  "text batch should compact/group failure reasons") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_win95_pack_text_batch_report(
      batch,
      "DATA",
      romulus::data::Win95PackTextBatchReportOptions{
          .preview_entry_limit = 2,
      });
  if (assert_true(report.find("# Caesar II Win95 PACK Text Batch Report") != std::string::npos,
                  "text batch report title should be stable") != 0) {
    return 1;
  }
  return assert_true(report.find("text_entries_preview_truncated: yes") != std::string::npos,
                     "text batch report should include deterministic truncation state");
}

int test_builds_pack_text_success_index_and_report() {
  const auto bytes = make_pack_with_text_batch_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "text index test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto batch = romulus::data::analyze_win95_pack_text_batch(bytes, parsed.value.value(), 10);
  const auto index = romulus::data::build_win95_pack_text_success_index(batch);
  if (assert_true(index.successful_entry_count == 2, "text index should include only successful entries") != 0) {
    return 1;
  }
  if (assert_true(index.successful_entries[0].entry_index == 0 && index.successful_entries[1].entry_index == 3,
                  "text index should preserve deterministic entry ordering") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_win95_pack_text_index_report(
      index,
      "DATA",
      romulus::data::Win95PackTextIndexReportOptions{
          .preview_entry_limit = 1,
      });
  if (assert_true(report.find("# Caesar II Win95 PACK Text Success Index Report") != std::string::npos,
                  "text index report title should be stable") != 0) {
    return 1;
  }
  return assert_true(report.find("successful_entries_preview_truncated: yes") != std::string::npos,
                     "text index report should expose deterministic truncation state");
}

int test_pack_text_index_lookup_and_export_rules() {
  const auto bytes = make_pack_with_text_batch_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "text lookup test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto batch = romulus::data::analyze_win95_pack_text_batch(bytes, parsed.value.value(), 32);
  const auto index = romulus::data::build_win95_pack_text_success_index(batch);

  const auto failed_lookup = romulus::data::find_win95_pack_text_index_entry(index, 1);
  if (assert_true(!failed_lookup.has_value(), "failed text entries should not be present in success index") != 0) {
    return 1;
  }
  const auto invalid_lookup = romulus::data::find_win95_pack_text_index_entry(index, 99);
  if (assert_true(!invalid_lookup.has_value(), "invalid text entry index should not resolve in success index") != 0) {
    return 1;
  }
  const auto success_lookup = romulus::data::find_win95_pack_text_index_entry(index, 3);
  if (assert_true(success_lookup.has_value(), "successful text entries should be index-exportable") != 0) {
    return 1;
  }

  const auto failed_extract = romulus::data::extract_win95_pack_text_entry(bytes, parsed.value.value(), 1);
  if (assert_true(!failed_extract.ok(), "non-decodable text-like entries should fail export extraction") != 0) {
    return 1;
  }
  return 0;
}

int test_extract_pack_pl8_entry_success_and_stable_report() {
  const auto bytes = make_pack_with_pl8_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "pl8 extraction test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_pl8_entry(bytes, parsed.value.value(), 0);
  if (assert_true(extracted.ok(), "768-byte opaque payload should extract as supported PL8") != 0) {
    return 1;
  }
  if (assert_true(extracted.value->pl8.palette_entries.size() == romulus::data::Pl8Resource::kSupportedEntryCount,
                  "pack PL8 extraction should decode 256 palette entries") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_pl8_report(extracted.value->pl8, 2);
  if (assert_true(report.find("# Caesar II Win95 PL8 Report") != std::string::npos,
                  "pack PL8 probe should reuse stable PL8 report formatter") != 0) {
    return 1;
  }
  return assert_true(report.find("palette_preview_count: 2") != std::string::npos,
                     "pack PL8 report preview bound should be deterministic");
}

int test_extract_pack_pl8_entry_rejects_invalid_index() {
  const auto bytes = make_pack_with_pl8_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "pack pl8 invalid index test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_pl8_entry(bytes, parsed.value.value(), 9);
  if (assert_true(!extracted.ok(), "invalid pl8 entry index should fail") != 0) {
    return 1;
  }
  return assert_true(extracted.error->message.find("Invalid PACK entry index") != std::string::npos,
                     "invalid pl8 index error should be explicit");
}

int test_extract_pack_pl8_entry_rejects_unsupported_family() {
  const auto bytes = make_pack_with_pl8_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "pack pl8 family rejection test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_pl8_entry(bytes, parsed.value.value(), 1);
  if (assert_true(!extracted.ok(), "non-768-byte payload should remain unsupported for PL8 extraction") != 0) {
    return 1;
  }
  return assert_true(extracted.error->message.find("unsupported for PL8 extraction") != std::string::npos,
                     "unsupported pl8-like family failure should be explicit");
}

int test_extract_pack_pl8_entry_rejects_truncated_payload_bounds() {
  const auto bytes = make_pack_with_pl8_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "pack pl8 truncated bounds test requires valid PACK parse") != 0) {
    return 1;
  }

  std::vector<std::uint8_t> truncated = bytes;
  truncated.resize(parsed.value->entries[0].end_offset - 1);
  const auto extracted = romulus::data::extract_win95_pack_pl8_entry(truncated, parsed.value.value(), 0);
  if (assert_true(!extracted.ok(), "truncated pl8 payload bounds should fail extraction") != 0) {
    return 1;
  }
  return assert_true(extracted.error->message.find("exceeds container bounds") != std::string::npos,
                     "truncated pl8 payload should report bounded extraction failure");
}

int test_extract_pack_pl8_entry_export_behavior() {
  const auto bytes = make_pack_with_pl8_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "pack pl8 export behavior test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_pl8_entry(bytes, parsed.value.value(), 0);
  if (assert_true(extracted.ok(), "pack pl8 export behavior test requires successful extraction") != 0) {
    return 1;
  }

  const auto output_path = std::filesystem::temp_directory_path() / "romulus_pack_pl8_export.pl8";
  std::ofstream output(output_path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (assert_true(output.is_open(), "pack pl8 export path should open temporary file") != 0) {
    return 1;
  }
  output.write(reinterpret_cast<const char*>(extracted.value->payload_bytes.data()),
               static_cast<std::streamsize>(extracted.value->payload_bytes.size()));
  output.close();
  if (assert_true(std::filesystem::exists(output_path), "pack pl8 export should create output file") != 0) {
    return 1;
  }
  const auto file_size = std::filesystem::file_size(output_path);
  std::filesystem::remove(output_path);
  return assert_true(file_size == romulus::data::Pl8Resource::kSupportedPayloadSize,
                     "pack pl8 export file size should match bounded 768-byte payload");
}

int test_builds_unified_pack_success_index_across_supported_families() {
  const auto bytes = make_pack_with_mixed_known_families_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "unified known index test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto index = romulus::data::build_win95_pack_unified_success_index(bytes, parsed.value.value(), 8);
  if (assert_true(index.summary.total_entry_count == 4, "unified summary should report total PACK entries") != 0) {
    return 1;
  }
  if (assert_true(index.summary.known_entry_count == 3, "unified summary should aggregate known family successes") != 0) {
    return 1;
  }
  if (assert_true(index.summary.unknown_entry_count == 1, "unified summary should include unknown entry count") != 0) {
    return 1;
  }
  if (assert_true(index.summary.ilbm_success_count == 1 && index.summary.text_success_count == 1 &&
                      index.summary.pl8_success_count == 1,
                  "unified summary should include deterministic per-family counts") != 0) {
    return 1;
  }
  if (assert_true(index.successful_entries.size() == 3, "unified success entries should include only successful decodes") != 0) {
    return 1;
  }
  if (assert_true(index.successful_entries[0].entry_index == 0 && index.successful_entries[1].entry_index == 1 &&
                      index.successful_entries[2].entry_index == 2,
                  "unified success entries should remain deterministic in entry index order") != 0) {
    return 1;
  }

  if (assert_true(index.successful_entries[0].ilbm_width.value_or(0) == 2 &&
                      index.successful_entries[0].ilbm_height.value_or(0) == 2,
                  "unified ILBM entry should include compact width and height metadata") != 0) {
    return 1;
  }
  if (assert_true(index.successful_entries[1].text_line_count.value_or(0) == 3 &&
                      index.successful_entries[1].text_character_count.value_or(0) == 11 &&
                      index.successful_entries[1].text_preview.value_or("").size() <= 8,
                  "unified text entry should include compact bounded preview metadata") != 0) {
    return 1;
  }
  return assert_true(index.successful_entries[2].pl8_palette_entry_count.value_or(0) ==
                         romulus::data::Pl8Resource::kSupportedEntryCount,
                     "unified PL8 entry should include supported layout palette count metadata");
}

int test_unified_pack_success_report_is_stable_and_bounded() {
  const auto bytes = make_pack_with_mixed_known_families_fixture();
  const auto parsed = romulus::data::parse_win95_pack_container(bytes);
  if (assert_true(parsed.ok(), "unified report test requires valid PACK parse") != 0) {
    return 1;
  }

  const auto index = romulus::data::build_win95_pack_unified_success_index(bytes, parsed.value.value(), 8);
  const auto compact_report = romulus::data::format_win95_pack_unified_success_index_report(
      index,
      "DATA",
      romulus::data::Win95PackUnifiedSuccessReportOptions{
          .preview_entry_limit = 2,
          .include_all_entries = false,
      });
  if (assert_true(compact_report.find("# Caesar II Win95 PACK Unified Success Index Report") != std::string::npos,
                  "unified report should include stable title") != 0) {
    return 1;
  }
  if (assert_true(compact_report.find("known_ratio: 3/4") != std::string::npos,
                  "unified report should include compact known coverage ratio") != 0) {
    return 1;
  }
  if (assert_true(compact_report.find("family_success_counts: ilbm=1 text=1 pl8=1") != std::string::npos,
                  "unified report should include deterministic per-family summary counts") != 0) {
    return 1;
  }
  if (assert_true(compact_report.find("successful_entries_preview: showing 2 of 3") != std::string::npos,
                  "unified report should honor bounded preview mode") != 0) {
    return 1;
  }
  if (assert_true(compact_report.find("successful_entries_preview_truncated: yes") != std::string::npos,
                  "unified report should mark bounded preview truncation") != 0) {
    return 1;
  }

  const auto all_report = romulus::data::format_win95_pack_unified_success_index_report(
      index,
      "DATA",
      romulus::data::Win95PackUnifiedSuccessReportOptions{
          .preview_entry_limit = 1,
          .include_all_entries = true,
      });
  if (assert_true(all_report.find("successful_entries_preview: showing 3 of 3") != std::string::npos,
                  "all-entry report mode should include every successful known entry") != 0) {
    return 1;
  }
  return assert_true(all_report.find("family=pl8") != std::string::npos &&
                         all_report.find("layout=simple-pl8") != std::string::npos,
                     "all-entry report should include compact PL8 metadata markers");
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

  if (test_batch_analyzes_only_possible_ilbm_entries() != 0) {
    return EXIT_FAILURE;
  }

  if (test_batch_report_formatting_and_preview_bounds() != 0) {
    return EXIT_FAILURE;
  }

  if (test_builds_successful_ilbm_index_and_stable_report() != 0) {
    return EXIT_FAILURE;
  }

  if (test_index_lookup_and_export_path_behavior() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_text_entry_success_and_preview_report() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_text_entry_rejects_invalid_index() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_text_entry_rejects_non_text_like_classification() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_text_entry_rejects_truncated_payload_bounds() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_text_entry_rejects_malformed_non_decodable_payload() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_text_export_path_behavior() != 0) {
    return EXIT_FAILURE;
  }

  if (test_batch_analyzes_text_like_entries_and_groups_failures() != 0) {
    return EXIT_FAILURE;
  }

  if (test_builds_pack_text_success_index_and_report() != 0) {
    return EXIT_FAILURE;
  }

  if (test_pack_text_index_lookup_and_export_rules() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_pl8_entry_success_and_stable_report() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_pl8_entry_rejects_invalid_index() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_pl8_entry_rejects_unsupported_family() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_pl8_entry_rejects_truncated_payload_bounds() != 0) {
    return EXIT_FAILURE;
  }

  if (test_extract_pack_pl8_entry_export_behavior() != 0) {
    return EXIT_FAILURE;
  }

  if (test_builds_unified_pack_success_index_across_supported_families() != 0) {
    return EXIT_FAILURE;
  }

  if (test_unified_pack_success_report_is_stable_and_bounded() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
