#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "romulus/data/indexed_image.h"
#include "romulus/data/pl8_sprite_table_resource.h"

namespace romulus::platform {

enum class SpritePlacementMode : std::uint8_t {
  TopLeft = 0,
  BottomLeft = 1,
  Centered = 2,
  BottomCenter = 3,
};

enum class SpriteDrawOrder : std::uint8_t {
  Forward = 0,
  Reverse = 1,
};

struct SpritePlacementRect {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;
};

struct SpritePlacementOptions {
  SpritePlacementMode placement_mode = SpritePlacementMode::TopLeft;
  SpriteDrawOrder draw_order = SpriteDrawOrder::Forward;
  std::optional<std::size_t> isolated_sprite_index;
};

struct SpritePlacementDebugEntry {
  std::size_t sprite_index = 0;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::int16_t descriptor_x = 0;
  std::int16_t descriptor_y = 0;
  std::uint16_t tile_type = 0;
  SpritePlacementRect destination_rect;
};

struct SpriteLayerPlacementStats {
  std::size_t placed_sprites = 0;
  std::size_t clipped_sprites = 0;
  std::size_t skipped_out_of_bounds_sprites = 0;
};

struct SpriteLayerPlacementResult {
  romulus::data::RgbaImage rgba_image;
  SpriteLayerPlacementStats stats;
  std::vector<SpritePlacementDebugEntry> debug_entries;
};

[[nodiscard]] std::vector<SpritePlacementMode> sprite_placement_modes();
[[nodiscard]] const char* sprite_placement_mode_name(SpritePlacementMode mode);
[[nodiscard]] const char* sprite_draw_order_name(SpriteDrawOrder order);
[[nodiscard]] SpritePlacementRect compute_sprite_destination_rect(const romulus::data::Pl8DecodedSprite& decoded,
                                                                  SpritePlacementMode mode);
[[nodiscard]] bool sprite_is_visible_for_options(std::size_t sprite_index,
                                                 const SpritePlacementOptions& options);
[[nodiscard]] std::string format_sprite_placement_report(
    const std::vector<romulus::data::Pl8DecodedSprite>& decoded_sprites,
    const SpritePlacementOptions& options,
    const std::vector<SpritePlacementDebugEntry>& debug_entries);

[[nodiscard]] std::optional<SpriteLayerPlacementResult> compose_sprite_layer_to_canvas(
    std::uint16_t canvas_width,
    std::uint16_t canvas_height,
    const std::vector<romulus::data::Pl8DecodedSprite>& decoded_sprites,
    const SpritePlacementOptions& options = {});

}  // namespace romulus::platform
