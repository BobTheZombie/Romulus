#include "romulus/data/binary_reader.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int assert_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    return 1;
  }

  return 0;
}

int test_little_endian_reads_and_tell() {
  const std::array<std::uint8_t, 9> bytes = {0x10, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0xAA, 0xBB};
  romulus::data::BinaryReader reader(bytes);

  const auto u8 = reader.read_u8();
  if (assert_true(u8.ok() && u8.value.value() == 0x10, "u8 read should succeed and match") != 0) {
    return 1;
  }

  const auto u16 = reader.read_u16_le();
  if (assert_true(u16.ok() && u16.value.value() == 0x1234, "u16 read should decode little-endian") != 0) {
    return 1;
  }

  const auto u32 = reader.read_u32_le();
  if (assert_true(u32.ok() && u32.value.value() == 0x12345678, "u32 read should decode little-endian") != 0) {
    return 1;
  }

  const auto span = reader.read_bytes(2);
  if (assert_true(span.ok(), "byte span read should succeed") != 0) {
    return 1;
  }

  const auto slice = span.value.value();
  if (assert_true(slice.size() == 2, "span size should match requested count") != 0) {
    return 1;
  }

  const auto b0 = std::to_integer<std::uint8_t>(slice[0]);
  const auto b1 = std::to_integer<std::uint8_t>(slice[1]);
  if (assert_true(b0 == 0xAA && b1 == 0xBB, "span bytes should preserve input values") != 0) {
    return 1;
  }

  return assert_true(reader.tell() == 9 && reader.remaining() == 0, "tell/remaining should advance after reads");
}

int test_out_of_bounds_reports_parse_error() {
  const std::array<std::uint8_t, 2> bytes = {0xFF, 0xEE};
  romulus::data::BinaryReader reader(bytes);

  const auto value = reader.read_u32_le();
  if (assert_true(!value.ok(), "out-of-bounds read should fail") != 0) {
    return 1;
  }

  if (assert_true(value.error.has_value(), "failed read should include parse error") != 0) {
    return 1;
  }

  const auto& error = value.error.value();
  if (assert_true(error.code == romulus::data::ParseErrorCode::OutOfBounds, "error code should be out-of-bounds") != 0) {
    return 1;
  }

  if (assert_true(error.offset == 0 && error.requested_bytes == 4 && error.buffer_size == 2,
                  "error should include offset/requested/size metadata") != 0) {
    return 1;
  }

  if (assert_true(error.message.find("Read out of bounds") != std::string::npos,
                  "error message should include useful context") != 0) {
    return 1;
  }

  return assert_true(reader.tell() == 0, "failed read should not advance offset");
}

int test_seek_and_read_behavior() {
  const std::array<std::uint8_t, 4> bytes = {0x00, 0x11, 0x22, 0x33};
  romulus::data::BinaryReader reader(bytes);

  const auto seek_ok = reader.seek(2);
  if (assert_true(!seek_ok.has_value(), "seek to valid offset should succeed") != 0) {
    return 1;
  }

  if (assert_true(reader.tell() == 2, "tell should return current seek offset") != 0) {
    return 1;
  }

  const auto byte = reader.read_u8();
  if (assert_true(byte.ok() && byte.value.value() == 0x22, "read after seek should start at seeked position") != 0) {
    return 1;
  }

  const auto seek_fail = reader.seek(99);
  if (assert_true(seek_fail.has_value(), "seek beyond end should fail") != 0) {
    return 1;
  }

  if (assert_true(seek_fail.value().code == romulus::data::ParseErrorCode::InvalidSeek,
                  "failed seek should report invalid seek code") != 0) {
    return 1;
  }

  return assert_true(reader.tell() == 3, "failed seek should preserve previous offset");
}

}  // namespace

int main() {
  if (test_little_endian_reads_and_tell() != 0) {
    return EXIT_FAILURE;
  }

  if (test_out_of_bounds_reports_parse_error() != 0) {
    return EXIT_FAILURE;
  }

  if (test_seek_and_read_behavior() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
