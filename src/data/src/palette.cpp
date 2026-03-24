#include "romulus/data/palette.h"

#include <algorithm>
#include <sstream>

namespace romulus::data {
namespace {

[[nodiscard]] std::uint8_t expand_6bit_to_8bit(std::uint8_t component) {
  return static_cast<std::uint8_t>((static_cast<std::uint16_t>(component) * 255U) / 63U);
}

[[nodiscard]] ParseError make_invalid_palette_error(std::size_t offset,
                                                    std::size_t buffer_size,
                                                    const std::string& message) {
  return make_invalid_format_error(offset, 0, buffer_size, message);
}

}  // namespace

ParseResult<PaletteResource> parse_palette_resource(std::span<const std::byte> bytes) {
  if (bytes.size() % PaletteResource::kBytesPerEntry != 0) {
    return {.error = make_invalid_palette_error(
                0,
                bytes.size(),
                "Palette byte size must be divisible by 3 for RGB triplets")};
  }

  const auto entry_count = bytes.size() / PaletteResource::kBytesPerEntry;
  if (entry_count != PaletteResource::kExpectedEntryCount) {
    std::ostringstream message;
    message << "Palette must contain exactly " << PaletteResource::kExpectedEntryCount << " entries; got "
            << entry_count;

    return {.error = make_invalid_palette_error(0, bytes.size(), message.str())};
  }

  BinaryReader reader(bytes);
  PaletteResource palette;
  palette.entries.reserve(entry_count);

  for (std::size_t index = 0; index < entry_count; ++index) {
    const auto red = reader.read_u8();
    if (!red.ok()) {
      return {.error = red.error};
    }

    const auto green = reader.read_u8();
    if (!green.ok()) {
      return {.error = green.error};
    }

    const auto blue = reader.read_u8();
    if (!blue.ok()) {
      return {.error = blue.error};
    }

    constexpr std::uint8_t kMaxVgaComponent = 63;
    if (red.value.value() > kMaxVgaComponent || green.value.value() > kMaxVgaComponent ||
        blue.value.value() > kMaxVgaComponent) {
      std::ostringstream message;
      message << "Palette entry " << index << " contains component outside VGA 6-bit range [0, 63]";

      return {.error = make_invalid_palette_error(
                  index * PaletteResource::kBytesPerEntry,
                  bytes.size(),
                  message.str())};
    }

    palette.entries.push_back(PaletteEntry{
        .red = red.value.value(),
        .green = green.value.value(),
        .blue = blue.value.value(),
    });
  }

  return {.value = std::move(palette)};
}

ParseResult<PaletteResource> parse_palette_resource(std::span<const std::uint8_t> bytes) {
  return parse_palette_resource(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
}

std::string format_palette_report(const PaletteResource& palette, std::size_t max_entries) {
  std::ostringstream output;
  output << "# Caesar II Palette Report\n";
  output << "entry_count: " << palette.entries.size() << "\n";

  const auto display_count = std::min(max_entries, palette.entries.size());
  for (std::size_t index = 0; index < display_count; ++index) {
    const auto& entry = palette.entries[index];
    output << "[" << index << "] " << static_cast<int>(entry.red) << "," << static_cast<int>(entry.green) << ","
           << static_cast<int>(entry.blue) << " -> " << static_cast<int>(expand_6bit_to_8bit(entry.red)) << ","
           << static_cast<int>(expand_6bit_to_8bit(entry.green)) << ","
           << static_cast<int>(expand_6bit_to_8bit(entry.blue)) << "\n";
  }

  if (display_count < palette.entries.size()) {
    output << "... (" << (palette.entries.size() - display_count) << " more entries)\n";
  }

  return output.str();
}

}  // namespace romulus::data
