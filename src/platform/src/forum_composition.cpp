#include "romulus/platform/forum_composition.h"

#include <algorithm>

namespace romulus::platform {

std::optional<SpriteLayerPlacementResult> compose_sprite_layer_to_canvas(
    const std::uint16_t canvas_width,
    const std::uint16_t canvas_height,
    const std::vector<romulus::data::Pl8DecodedSprite>& decoded_sprites) {
  if (canvas_width == 0 || canvas_height == 0) {
    return std::nullopt;
  }

  SpriteLayerPlacementResult result;
  result.rgba_image.width = canvas_width;
  result.rgba_image.height = canvas_height;
  result.rgba_image.pixels_rgba.assign(static_cast<std::size_t>(canvas_width) * canvas_height * 4U, 0);

  const auto canvas_w = static_cast<std::int32_t>(canvas_width);
  const auto canvas_h = static_cast<std::int32_t>(canvas_height);

  for (const auto& decoded : decoded_sprites) {
    const auto sprite_w = static_cast<std::int32_t>(decoded.sprite.width);
    const auto sprite_h = static_cast<std::int32_t>(decoded.sprite.height);
    if (sprite_w <= 0 || sprite_h <= 0) {
      ++result.stats.skipped_out_of_bounds_sprites;
      continue;
    }

    const auto src_x0 = static_cast<std::int32_t>(decoded.sprite.x);
    const auto src_y0 = static_cast<std::int32_t>(decoded.sprite.y);
    const auto src_x1 = src_x0 + sprite_w;
    const auto src_y1 = src_y0 + sprite_h;

    const auto dst_x0 = std::max<std::int32_t>(0, src_x0);
    const auto dst_y0 = std::max<std::int32_t>(0, src_y0);
    const auto dst_x1 = std::min<std::int32_t>(canvas_w, src_x1);
    const auto dst_y1 = std::min<std::int32_t>(canvas_h, src_y1);

    if (dst_x0 >= dst_x1 || dst_y0 >= dst_y1) {
      ++result.stats.skipped_out_of_bounds_sprites;
      continue;
    }

    ++result.stats.placed_sprites;
    if (dst_x0 != src_x0 || dst_y0 != src_y0 || dst_x1 != src_x1 || dst_y1 != src_y1) {
      ++result.stats.clipped_sprites;
    }

    const auto copy_w = static_cast<std::size_t>(dst_x1 - dst_x0);
    const auto copy_h = static_cast<std::size_t>(dst_y1 - dst_y0);
    const auto src_start_x = static_cast<std::size_t>(dst_x0 - src_x0);
    const auto src_start_y = static_cast<std::size_t>(dst_y0 - src_y0);
    const auto sprite_width = static_cast<std::size_t>(decoded.sprite.width);
    const auto canvas_width_size = static_cast<std::size_t>(canvas_width);

    for (std::size_t y = 0; y < copy_h; ++y) {
      for (std::size_t x = 0; x < copy_w; ++x) {
        const auto src_x = src_start_x + x;
        const auto src_y = src_start_y + y;
        const auto dst_x = static_cast<std::size_t>(dst_x0) + x;
        const auto dst_y = static_cast<std::size_t>(dst_y0) + y;

        const auto src_offset = (src_y * sprite_width + src_x) * 4U;
        const auto dst_offset = (dst_y * canvas_width_size + dst_x) * 4U;
        const auto alpha = decoded.rgba_image.pixels_rgba[src_offset + 3];
        if (alpha == 0) {
          continue;
        }

        result.rgba_image.pixels_rgba[dst_offset] = decoded.rgba_image.pixels_rgba[src_offset];
        result.rgba_image.pixels_rgba[dst_offset + 1] = decoded.rgba_image.pixels_rgba[src_offset + 1];
        result.rgba_image.pixels_rgba[dst_offset + 2] = decoded.rgba_image.pixels_rgba[src_offset + 2];
        result.rgba_image.pixels_rgba[dst_offset + 3] = alpha;
      }
    }
  }

  return result;
}

}  // namespace romulus::platform
