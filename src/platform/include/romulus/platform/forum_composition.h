#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "romulus/data/indexed_image.h"
#include "romulus/data/pl8_sprite_table_resource.h"

namespace romulus::platform {

struct SpriteLayerPlacementStats {
  std::size_t placed_sprites = 0;
  std::size_t clipped_sprites = 0;
  std::size_t skipped_out_of_bounds_sprites = 0;
};

struct SpriteLayerPlacementResult {
  romulus::data::RgbaImage rgba_image;
  SpriteLayerPlacementStats stats;
};

[[nodiscard]] std::optional<SpriteLayerPlacementResult> compose_sprite_layer_to_canvas(
    std::uint16_t canvas_width,
    std::uint16_t canvas_height,
    const std::vector<romulus::data::Pl8DecodedSprite>& decoded_sprites);

}  // namespace romulus::platform
