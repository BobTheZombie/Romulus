#include "romulus/data/binary_reader.h"

#include <sstream>

namespace romulus::data {

ParseError make_out_of_bounds_error(std::size_t offset, std::size_t requested_bytes, std::size_t buffer_size) {
  std::ostringstream message;
  message << "Read out of bounds: offset=" << offset << ", requested_bytes=" << requested_bytes
          << ", buffer_size=" << buffer_size;

  ParseError error;
  error.code = ParseErrorCode::OutOfBounds;
  error.offset = offset;
  error.requested_bytes = requested_bytes;
  error.buffer_size = buffer_size;
  error.message = message.str();
  return error;
}

ParseError make_invalid_seek_error(std::size_t requested_offset, std::size_t buffer_size) {
  std::ostringstream message;
  message << "Invalid seek: requested_offset=" << requested_offset << ", buffer_size=" << buffer_size;

  ParseError error;
  error.code = ParseErrorCode::InvalidSeek;
  error.offset = requested_offset;
  error.requested_bytes = 0;
  error.buffer_size = buffer_size;
  error.message = message.str();
  return error;
}

BinaryReader::BinaryReader(std::span<const std::byte> bytes) : bytes_(bytes) {}

BinaryReader::BinaryReader(std::span<const std::uint8_t> bytes)
    : bytes_(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()) {}

std::size_t BinaryReader::size() const {
  return bytes_.size();
}

std::size_t BinaryReader::tell() const {
  return offset_;
}

std::size_t BinaryReader::remaining() const {
  return bytes_.size() - offset_;
}

std::optional<ParseError> BinaryReader::seek(std::size_t new_offset) {
  if (new_offset > bytes_.size()) {
    return make_invalid_seek_error(new_offset, bytes_.size());
  }

  offset_ = new_offset;
  return std::nullopt;
}

ParseResult<std::uint8_t> BinaryReader::read_u8() {
  const auto bounds = check_bounds(sizeof(std::uint8_t));
  if (bounds.has_value()) {
    return {.error = bounds};
  }

  const auto value = std::to_integer<std::uint8_t>(bytes_[offset_]);
  offset_ += sizeof(std::uint8_t);
  return {.value = value};
}

ParseResult<std::uint16_t> BinaryReader::read_u16_le() {
  const auto bounds = check_bounds(sizeof(std::uint16_t));
  if (bounds.has_value()) {
    return {.error = bounds};
  }

  const auto b0 = static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes_[offset_]));
  const auto b1 = static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes_[offset_ + 1]));
  const auto value = static_cast<std::uint16_t>(b0 | (b1 << 8));

  offset_ += sizeof(std::uint16_t);
  return {.value = value};
}

ParseResult<std::uint32_t> BinaryReader::read_u32_le() {
  const auto bounds = check_bounds(sizeof(std::uint32_t));
  if (bounds.has_value()) {
    return {.error = bounds};
  }

  const auto b0 = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[offset_]));
  const auto b1 = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[offset_ + 1]));
  const auto b2 = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[offset_ + 2]));
  const auto b3 = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[offset_ + 3]));
  const auto value = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);

  offset_ += sizeof(std::uint32_t);
  return {.value = value};
}

ParseResult<std::span<const std::byte>> BinaryReader::read_bytes(std::size_t count) {
  const auto bounds = check_bounds(count);
  if (bounds.has_value()) {
    return {.error = bounds};
  }

  const auto segment = bytes_.subspan(offset_, count);
  offset_ += count;
  return {.value = segment};
}

std::optional<ParseError> BinaryReader::check_bounds(std::size_t count) const {
  if (count <= remaining()) {
    return std::nullopt;
  }

  return make_out_of_bounds_error(offset_, count, bytes_.size());
}

}  // namespace romulus::data
