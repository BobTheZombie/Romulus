#pragma once

#include <cstddef>
#include <string>

namespace romulus::data {

enum class ParseErrorCode {
  OutOfBounds,
  InvalidSeek,
};

struct ParseError {
  ParseErrorCode code = ParseErrorCode::OutOfBounds;
  std::size_t offset = 0;
  std::size_t requested_bytes = 0;
  std::size_t buffer_size = 0;
  std::string message;
};

[[nodiscard]] ParseError make_out_of_bounds_error(std::size_t offset, std::size_t requested_bytes, std::size_t buffer_size);
[[nodiscard]] ParseError make_invalid_seek_error(std::size_t requested_offset, std::size_t buffer_size);

}  // namespace romulus::data
