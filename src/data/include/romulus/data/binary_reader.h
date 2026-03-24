#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "romulus/data/parse_error.h"

namespace romulus::data {

template <typename T>
struct ParseResult {
  std::optional<T> value;
  std::optional<ParseError> error;

  [[nodiscard]] bool ok() const {
    return value.has_value();
  }
};

class BinaryReader {
 public:
  explicit BinaryReader(std::span<const std::byte> bytes);
  explicit BinaryReader(std::span<const std::uint8_t> bytes);

  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] std::size_t tell() const;
  [[nodiscard]] std::size_t remaining() const;

  [[nodiscard]] std::optional<ParseError> seek(std::size_t new_offset);

  [[nodiscard]] ParseResult<std::uint8_t> read_u8();
  [[nodiscard]] ParseResult<std::uint16_t> read_u16_le();
  [[nodiscard]] ParseResult<std::uint32_t> read_u32_le();
  [[nodiscard]] ParseResult<std::span<const std::byte>> read_bytes(std::size_t count);

 private:
  [[nodiscard]] std::optional<ParseError> check_bounds(std::size_t count) const;

  std::span<const std::byte> bytes_;
  std::size_t offset_ = 0;
};

}  // namespace romulus::data
