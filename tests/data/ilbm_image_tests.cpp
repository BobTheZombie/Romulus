#include "romulus/data/ilbm_image.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
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

std::optional<std::size_t> find_chunk_data_offset(const std::vector<std::uint8_t>& bytes, const char id[4]) {
  if (bytes.size() < 12) {
    return std::nullopt;
  }
  std::size_t offset = 12;
  while (offset + 8 <= bytes.size()) {
    if (bytes[offset] == static_cast<std::uint8_t>(id[0]) && bytes[offset + 1] == static_cast<std::uint8_t>(id[1]) &&
        bytes[offset + 2] == static_cast<std::uint8_t>(id[2]) && bytes[offset + 3] == static_cast<std::uint8_t>(id[3])) {
      return offset + 8;
    }
    const auto size = (static_cast<std::uint32_t>(bytes[offset + 4]) << 24U) |
                      (static_cast<std::uint32_t>(bytes[offset + 5]) << 16U) |
                      (static_cast<std::uint32_t>(bytes[offset + 6]) << 8U) |
                      static_cast<std::uint32_t>(bytes[offset + 7]);
    offset += 8 + size + (size & 1U);
  }
  return std::nullopt;
}

std::vector<std::uint8_t> make_fixture_ilbm() {
  std::vector<std::uint8_t> bmhd;
  append_u16_be(bmhd, 2);   // width
  append_u16_be(bmhd, 2);   // height
  append_u16_be(bmhd, 0);   // x
  append_u16_be(bmhd, 0);   // y
  bmhd.push_back(8);        // nPlanes
  bmhd.push_back(0);        // masking
  bmhd.push_back(0);        // compression
  bmhd.push_back(0);        // pad1
  append_u16_be(bmhd, 0);   // transparentColor
  bmhd.push_back(10);       // xAspect
  bmhd.push_back(11);       // yAspect
  append_u16_be(bmhd, 2);   // pageWidth
  append_u16_be(bmhd, 2);   // pageHeight

  std::vector<std::uint8_t> cmap = {
      0, 0, 0,
      20, 30, 40,
      40, 50, 60,
      70, 80, 90,
      100, 110, 120,
  };

  std::vector<std::uint8_t> body = {
      0x80, 0x00,  // row0 plane0 : pixel0 bit1, pixel1 bit0
      0x40, 0x00,  // row0 plane1 : pixel0 bit0, pixel1 bit1
      0x00, 0x00,
      0x00, 0x00,
      0x00, 0x00,
      0x00, 0x00,
      0x00, 0x00,
      0x00, 0x00,
      0x80, 0x00,  // row1 plane0 : pixel0 bit1, pixel1 bit0
      0x80, 0x00,  // row1 plane1 : pixel0 bit1, pixel1 bit0
      0x40, 0x00,  // row1 plane2 : pixel0 bit0, pixel1 bit1
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

std::vector<std::uint8_t> make_fixture_pbm(bool compressed = false) {
  std::vector<std::uint8_t> bmhd;
  append_u16_be(bmhd, 2);   // width
  append_u16_be(bmhd, 2);   // height
  append_u16_be(bmhd, 0);   // x
  append_u16_be(bmhd, 0);   // y
  bmhd.push_back(8);        // nPlanes/depth
  bmhd.push_back(0);        // masking
  bmhd.push_back(compressed ? 1 : 0);  // compression
  bmhd.push_back(0);        // pad1
  append_u16_be(bmhd, 0);   // transparentColor
  bmhd.push_back(10);       // xAspect
  bmhd.push_back(11);       // yAspect
  append_u16_be(bmhd, 2);   // pageWidth
  append_u16_be(bmhd, 2);   // pageHeight

  std::vector<std::uint8_t> cmap = {
      0, 0, 0,
      20, 30, 40,
      40, 50, 60,
      70, 80, 90,
      100, 110, 120,
  };

  std::vector<std::uint8_t> body = {1, 2, 3, 4};
  if (compressed) {
    body = {3, 1, 2, 3, 4};  // ByteRun1 literal run of four bytes
  }

  std::vector<std::uint8_t> form_payload;
  form_payload.insert(form_payload.end(), {'P', 'B', 'M', ' '});
  append_chunk(form_payload, "BMHD", bmhd);
  append_chunk(form_payload, "CMAP", cmap);
  append_chunk(form_payload, "BODY", body);

  std::vector<std::uint8_t> bytes = {'F', 'O', 'R', 'M'};
  append_u32_be(bytes, static_cast<std::uint32_t>(form_payload.size()));
  bytes.insert(bytes.end(), form_payload.begin(), form_payload.end());
  return bytes;
}

int test_parse_ilbm_success() {
  const auto bytes = make_fixture_ilbm();
  const auto parsed = romulus::data::parse_ilbm_image(bytes);
  if (assert_true(parsed.ok(), "valid ILBM fixture should parse") != 0) {
    return 1;
  }

  const auto& image = parsed.value.value();
  if (assert_true(image.width == 2 && image.height == 2, "decoded dimensions should match fixture") != 0) {
    return 1;
  }
  if (assert_true(image.plane_count == 8, "decoded plane count should match fixture") != 0) {
    return 1;
  }
  if (assert_true(image.palette_entries.size() == 5, "decoded palette entries should match CMAP") != 0) {
    return 1;
  }
  if (assert_true(image.indexed_pixels.size() == 4, "decoded indexed pixel count should match dimensions") != 0) {
    return 1;
  }
  return assert_true(image.indexed_pixels[0] == 1 && image.indexed_pixels[1] == 2 && image.indexed_pixels[2] == 3 &&
                         image.indexed_pixels[3] == 4,
                     "decoded indexed pixels should match planar BODY bits");
}

int test_parse_pbm_success() {
  const auto bytes = make_fixture_pbm();
  const auto parsed = romulus::data::parse_ilbm_image(bytes);
  if (assert_true(parsed.ok(), "valid PBM fixture should parse") != 0) {
    return 1;
  }

  const auto& image = parsed.value.value();
  if (assert_true(image.form_subtype == romulus::data::LbmFormSubtype::Pbm,
                  "PBM FORM subtype should be recorded") != 0) {
    return 1;
  }
  if (assert_true(image.decode_mode == romulus::data::LbmDecodeMode::PackedPixels,
                  "PBM decode mode should be packed") != 0) {
    return 1;
  }
  if (assert_true(image.indexed_pixels.size() == 4 && image.indexed_pixels[0] == 1 && image.indexed_pixels[3] == 4,
                  "PBM packed BODY should decode as indexed pixels") != 0) {
    return 1;
  }

  const auto rgba = romulus::data::convert_ilbm_to_rgba(image);
  return assert_true(rgba.ok(), "PBM indexed output should reuse RGBA conversion path");
}

int test_parse_pbm_byterun1_success() {
  const auto bytes = make_fixture_pbm(true);
  const auto parsed = romulus::data::parse_ilbm_image(bytes);
  if (assert_true(parsed.ok(), "compressed PBM fixture should parse") != 0) {
    return 1;
  }
  const auto& image = parsed.value.value();
  return assert_true(image.indexed_pixels.size() == 4 && image.indexed_pixels[1] == 2 && image.indexed_pixels[2] == 3,
                     "compressed PBM BODY should decode through ByteRun1");
}

int test_parse_pbm_rejects_malformed_bmhd() {
  auto bytes = make_fixture_pbm();
  const auto bmhd = find_chunk_data_offset(bytes, "BMHD");
  if (assert_true(bmhd.has_value(), "fixture should contain BMHD") != 0) {
    return 1;
  }
  bytes[bmhd.value() + 8] = 4;  // BMHD depth byte
  const auto parsed = romulus::data::parse_ilbm_image(bytes);
  if (assert_true(!parsed.ok(), "unsupported PBM depth should fail") != 0) {
    return 1;
  }
  return assert_true(parsed.error.has_value(), "unsupported PBM depth should provide error");
}

int test_parse_pbm_rejects_missing_cmap() {
  auto bytes = make_fixture_pbm();
  const auto cmap = find_chunk_data_offset(bytes, "CMAP");
  if (assert_true(cmap.has_value(), "fixture should contain CMAP") != 0) {
    return 1;
  }
  const auto chunk_offset = cmap.value() - 8;
  const auto size = (static_cast<std::uint32_t>(bytes[chunk_offset + 4]) << 24U) |
                    (static_cast<std::uint32_t>(bytes[chunk_offset + 5]) << 16U) |
                    (static_cast<std::uint32_t>(bytes[chunk_offset + 6]) << 8U) |
                    static_cast<std::uint32_t>(bytes[chunk_offset + 7]);
  const auto total_size = 8 + size + (size & 1U);
  bytes.erase(bytes.begin() + static_cast<std::ptrdiff_t>(chunk_offset),
              bytes.begin() + static_cast<std::ptrdiff_t>(chunk_offset + total_size));
  const auto form_size = static_cast<std::uint32_t>(bytes.size() - 8);
  bytes[4] = static_cast<std::uint8_t>((form_size >> 24U) & 0xFFU);
  bytes[5] = static_cast<std::uint8_t>((form_size >> 16U) & 0xFFU);
  bytes[6] = static_cast<std::uint8_t>((form_size >> 8U) & 0xFFU);
  bytes[7] = static_cast<std::uint8_t>(form_size & 0xFFU);
  const auto parsed = romulus::data::parse_ilbm_image(bytes);
  return assert_true(!parsed.ok(), "missing PBM CMAP should fail");
}

int test_parse_pbm_rejects_unsupported_layout() {
  auto bytes = make_fixture_pbm();
  const auto bmhd = find_chunk_data_offset(bytes, "BMHD");
  if (assert_true(bmhd.has_value(), "fixture should contain BMHD") != 0) {
    return 1;
  }
  bytes[bmhd.value() + 9] = 1;  // BMHD masking
  const auto parsed = romulus::data::parse_ilbm_image(bytes);
  if (assert_true(!parsed.ok(), "masked PBM layout should fail") != 0) {
    return 1;
  }
  return assert_true(parsed.error.has_value(), "masked PBM should provide error");
}

int test_parse_pbm_rejects_missing_body() {
  auto bytes = make_fixture_pbm();
  const auto body = find_chunk_data_offset(bytes, "BODY");
  if (assert_true(body.has_value(), "fixture should contain BODY") != 0) {
    return 1;
  }
  const auto chunk_offset = body.value() - 8;
  const auto size = (static_cast<std::uint32_t>(bytes[chunk_offset + 4]) << 24U) |
                    (static_cast<std::uint32_t>(bytes[chunk_offset + 5]) << 16U) |
                    (static_cast<std::uint32_t>(bytes[chunk_offset + 6]) << 8U) |
                    static_cast<std::uint32_t>(bytes[chunk_offset + 7]);
  const auto total_size = 8 + size + (size & 1U);
  bytes.erase(bytes.begin() + static_cast<std::ptrdiff_t>(chunk_offset),
              bytes.begin() + static_cast<std::ptrdiff_t>(chunk_offset + total_size));
  const auto form_size = static_cast<std::uint32_t>(bytes.size() - 8);
  bytes[4] = static_cast<std::uint8_t>((form_size >> 24U) & 0xFFU);
  bytes[5] = static_cast<std::uint8_t>((form_size >> 16U) & 0xFFU);
  bytes[6] = static_cast<std::uint8_t>((form_size >> 8U) & 0xFFU);
  bytes[7] = static_cast<std::uint8_t>(form_size & 0xFFU);
  const auto parsed = romulus::data::parse_ilbm_image(bytes);
  return assert_true(!parsed.ok(), "missing PBM BODY should fail");
}

int test_parse_pbm_rejects_truncated_body() {
  auto bytes = make_fixture_pbm();
  bytes.pop_back();
  const auto form_size = static_cast<std::uint32_t>(bytes.size() - 8);
  bytes[4] = static_cast<std::uint8_t>((form_size >> 24U) & 0xFFU);
  bytes[5] = static_cast<std::uint8_t>((form_size >> 16U) & 0xFFU);
  bytes[6] = static_cast<std::uint8_t>((form_size >> 8U) & 0xFFU);
  bytes[7] = static_cast<std::uint8_t>(form_size & 0xFFU);
  const auto parsed = romulus::data::parse_ilbm_image(bytes);
  return assert_true(!parsed.ok(), "truncated PBM BODY should fail");
}

int test_parse_ilbm_rejects_missing_body() {
  auto bytes = make_fixture_ilbm();
  bytes.resize(bytes.size() - (8 + 32));
  const auto form_size = static_cast<std::uint32_t>(bytes.size() - 8);
  bytes[4] = static_cast<std::uint8_t>((form_size >> 24U) & 0xFFU);
  bytes[5] = static_cast<std::uint8_t>((form_size >> 16U) & 0xFFU);
  bytes[6] = static_cast<std::uint8_t>((form_size >> 8U) & 0xFFU);
  bytes[7] = static_cast<std::uint8_t>(form_size & 0xFFU);

  const auto parsed = romulus::data::parse_ilbm_image(bytes);
  if (assert_true(!parsed.ok(), "missing BODY chunk should fail") != 0) {
    return 1;
  }
  return assert_true(parsed.error.has_value() &&
                         parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                     "missing BODY should return invalid format");
}

int test_parse_ilbm_rejects_non_ilbm_form() {
  auto bytes = make_fixture_ilbm();
  bytes[8] = 'A';
  bytes[9] = 'N';
  bytes[10] = 'I';
  bytes[11] = 'M';

  const auto parsed = romulus::data::parse_ilbm_image(bytes);
  if (assert_true(!parsed.ok(), "non-ILBM form type should fail") != 0) {
    return 1;
  }
  return assert_true(parsed.error.has_value() && parsed.error->offset == 8,
                     "non-ILBM form type should report FORM type offset");
}

int test_parse_ilbm_rejects_unsupported_planes() {
  auto bytes = make_fixture_ilbm();
  bytes[28] = 5;

  const auto parsed = romulus::data::parse_ilbm_image(bytes);
  if (assert_true(!parsed.ok(), "unsupported bitplane count should fail") != 0) {
    return 1;
  }
  return assert_true(parsed.error.has_value() &&
                         parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                     "unsupported bitplane count should map to invalid format");
}

int test_parse_ilbm_rejects_truncated_chunk_payload() {
  auto bytes = make_fixture_ilbm();
  bytes.pop_back();

  const auto parsed = romulus::data::parse_ilbm_image(bytes);
  if (assert_true(!parsed.ok(), "truncated ILBM payload should fail") != 0) {
    return 1;
  }
  return assert_true(parsed.error.has_value() &&
                         parsed.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                     "truncated payload should map to invalid format");
}

int test_lbm_report_stable_heading() {
  const auto parsed = romulus::data::parse_ilbm_image(make_fixture_ilbm());
  if (assert_true(parsed.ok(), "report test requires valid ILBM") != 0) {
    return 1;
  }

  const auto report = romulus::data::format_lbm_report(parsed.value.value(), 2);
  if (assert_true(report.find("# Caesar II Win95 LBM Report") != std::string::npos,
                  "report should include stable heading") != 0) {
    return 1;
  }
  if (assert_true(report.find("form_subtype: ILBM") != std::string::npos,
                  "report should include stable FORM subtype") != 0) {
    return 1;
  }
  return assert_true(report.find("... (3 more entries)") != std::string::npos,
                     "report should include palette truncation text");
}

int test_lbm_report_pbm_mode() {
  const auto parsed = romulus::data::parse_ilbm_image(make_fixture_pbm());
  if (assert_true(parsed.ok(), "PBM report test requires valid PBM") != 0) {
    return 1;
  }
  const auto report = romulus::data::format_lbm_report(parsed.value.value(), 2);
  if (assert_true(report.find("form_subtype: PBM ") != std::string::npos,
                  "report should include PBM subtype") != 0) {
    return 1;
  }
  return assert_true(report.find("decode_mode: packed") != std::string::npos,
                     "report should include packed decode mode");
}

int test_convert_ilbm_to_rgba_success() {
  const auto parsed = romulus::data::parse_ilbm_image(make_fixture_ilbm());
  if (assert_true(parsed.ok(), "RGBA conversion test requires valid ILBM") != 0) {
    return 1;
  }

  const auto rgba = romulus::data::convert_ilbm_to_rgba(parsed.value.value());
  if (assert_true(rgba.ok(), "ILBM with matching palette indices should convert to RGBA") != 0) {
    return 1;
  }

  const auto& image = rgba.value.value();
  if (assert_true(image.width == 2 && image.height == 2, "converted RGBA dimensions should match ILBM") != 0) {
    return 1;
  }

  if (assert_true(image.pixels_rgba.size() == 16, "2x2 RGBA image should have 16 bytes") != 0) {
    return 1;
  }

  return assert_true(
      image.pixels_rgba[0] == 20 && image.pixels_rgba[1] == 30 && image.pixels_rgba[2] == 40 &&
          image.pixels_rgba[3] == 255 && image.pixels_rgba[4] == 40 && image.pixels_rgba[5] == 50 &&
          image.pixels_rgba[6] == 60 && image.pixels_rgba[7] == 255,
      "converted RGBA bytes should follow decoded indexed pixels and CMAP entries");
}

int test_convert_ilbm_to_rgba_rejects_missing_palette_index() {
  auto parsed = romulus::data::parse_ilbm_image(make_fixture_ilbm());
  if (assert_true(parsed.ok(), "palette index failure test requires valid ILBM") != 0) {
    return 1;
  }

  auto image = parsed.value.value();
  image.palette_entries.resize(3);
  const auto rgba = romulus::data::convert_ilbm_to_rgba(image);
  if (assert_true(!rgba.ok(), "missing CMAP entry should fail conversion") != 0) {
    return 1;
  }

  return assert_true(rgba.error.has_value() &&
                         rgba.error->code == romulus::data::ParseErrorCode::InvalidFormat,
                     "missing CMAP entry should produce invalid format error");
}

}  // namespace

int main() {
  if (test_parse_ilbm_success() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_pbm_success() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_pbm_byterun1_success() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_pbm_rejects_malformed_bmhd() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_pbm_rejects_missing_cmap() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_pbm_rejects_unsupported_layout() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_pbm_rejects_missing_body() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_pbm_rejects_truncated_body() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_ilbm_rejects_missing_body() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_ilbm_rejects_non_ilbm_form() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_ilbm_rejects_unsupported_planes() != 0) {
    return EXIT_FAILURE;
  }
  if (test_parse_ilbm_rejects_truncated_chunk_payload() != 0) {
    return EXIT_FAILURE;
  }
  if (test_lbm_report_stable_heading() != 0) {
    return EXIT_FAILURE;
  }
  if (test_lbm_report_pbm_mode() != 0) {
    return EXIT_FAILURE;
  }
  if (test_convert_ilbm_to_rgba_success() != 0) {
    return EXIT_FAILURE;
  }
  if (test_convert_ilbm_to_rgba_rejects_missing_palette_index() != 0) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
