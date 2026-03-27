#include "romulus/platform/forum_composition.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

namespace romulus::platform {

std::vector<SpritePlacementMode> sprite_placement_modes() {
  return {SpritePlacementMode::TopLeft, SpritePlacementMode::BottomLeft, SpritePlacementMode::Centered,
          SpritePlacementMode::TopCenter, SpritePlacementMode::BottomCenter};
}

const char* sprite_placement_mode_name(const SpritePlacementMode mode) {
  switch (mode) {
    case SpritePlacementMode::TopLeft:
      return "top_left";
    case SpritePlacementMode::BottomLeft:
      return "bottom_left";
    case SpritePlacementMode::Centered:
      return "centered";
    case SpritePlacementMode::TopCenter:
      return "top_center";
    case SpritePlacementMode::BottomCenter:
      return "bottom_center";
  }

  return "unknown";
}

const char* sprite_draw_order_name(const SpriteDrawOrder order) {
  return order == SpriteDrawOrder::Reverse ? "reverse" : "forward";
}

SpritePlacementRect compute_sprite_destination_rect(const romulus::data::Pl8DecodedSprite& decoded,
                                                    const SpritePlacementMode mode) {
  const auto width = static_cast<std::int32_t>(decoded.sprite.width);
  const auto height = static_cast<std::int32_t>(decoded.sprite.height);
  const auto x = static_cast<std::int32_t>(decoded.sprite.x);
  const auto y = static_cast<std::int32_t>(decoded.sprite.y);

  switch (mode) {
    case SpritePlacementMode::TopLeft:
      return {.x = x, .y = y, .width = width, .height = height};
    case SpritePlacementMode::BottomLeft:
      return {.x = x, .y = y - height, .width = width, .height = height};
    case SpritePlacementMode::Centered:
      return {.x = x - (width / 2), .y = y - (height / 2), .width = width, .height = height};
    case SpritePlacementMode::TopCenter:
      return {.x = x - (width / 2), .y = y, .width = width, .height = height};
    case SpritePlacementMode::BottomCenter:
      return {.x = x - (width / 2), .y = y - height, .width = width, .height = height};
  }

  return {.x = x, .y = y, .width = width, .height = height};
}

SpritePlacementMode resolve_sprite_placement_mode(const std::string& asset_name,
                                                  const std::size_t sprite_index,
                                                  const SpritePlacementMode fallback_mode) {
  std::string normalized_file = std::filesystem::path(asset_name).filename().string();
  std::transform(normalized_file.begin(), normalized_file.end(), normalized_file.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  if (normalized_file == "rat_back.pl8") {
    if (sprite_index == 0) {
      return SpritePlacementMode::TopCenter;
    }
    if (sprite_index == 1 || sprite_index == 2) {
      return SpritePlacementMode::BottomCenter;
    }
  }

  return fallback_mode;
}

bool sprite_is_visible_for_options(const std::size_t sprite_index, const SpritePlacementOptions& options) {
  return !options.isolated_sprite_index.has_value() || options.isolated_sprite_index.value() == sprite_index;
}

std::string format_sprite_placement_report(const std::vector<romulus::data::Pl8DecodedSprite>& decoded_sprites,
                                           const SpritePlacementOptions& options,
                                           const std::vector<SpritePlacementDebugEntry>& debug_entries) {
  std::ostringstream out;
  out << "sprite_placement_report: mode=" << sprite_placement_mode_name(options.placement_mode)
      << " draw_order=" << sprite_draw_order_name(options.draw_order) << " isolate=";
  if (options.isolated_sprite_index.has_value()) {
    out << options.isolated_sprite_index.value();
  } else {
    out << "all";
  }
  out << " sprite_count=" << decoded_sprites.size() << "\n";

  for (const auto& entry : debug_entries) {
    out << "sprite[" << entry.sprite_index << "]: w=" << entry.width << " h=" << entry.height
        << " x=" << entry.descriptor_x << " y=" << entry.descriptor_y << " tile_type=" << entry.tile_type
        << " mode=" << sprite_placement_mode_name(entry.resolved_mode)
        << " dest=(" << entry.destination_rect.x << "," << entry.destination_rect.y << ","
        << entry.destination_rect.width << "x" << entry.destination_rect.height << ")\n";
  }

  return out.str();
}

std::optional<SpriteLayerPlacementResult> compose_sprite_layer_to_canvas(
    const std::uint16_t canvas_width,
    const std::uint16_t canvas_height,
    const std::vector<romulus::data::Pl8DecodedSprite>& decoded_sprites,
    const SpritePlacementOptions& options,
    const std::string& asset_name) {
  if (canvas_width == 0 || canvas_height == 0) {
    return std::nullopt;
  }

  SpriteLayerPlacementResult result;
  result.rgba_image.width = canvas_width;
  result.rgba_image.height = canvas_height;
  result.rgba_image.pixels_rgba.assign(static_cast<std::size_t>(canvas_width) * canvas_height * 4U, 0);

  const auto canvas_w = static_cast<std::int32_t>(canvas_width);
  const auto canvas_h = static_cast<std::int32_t>(canvas_height);

  std::vector<std::size_t> draw_indices;
  draw_indices.reserve(decoded_sprites.size());
  for (std::size_t i = 0; i < decoded_sprites.size(); ++i) {
    draw_indices.push_back(i);
  }
  if (options.draw_order == SpriteDrawOrder::Reverse) {
    std::reverse(draw_indices.begin(), draw_indices.end());
  }

  for (const auto draw_index : draw_indices) {
    const auto& decoded = decoded_sprites[draw_index];
    if (!sprite_is_visible_for_options(decoded.sprite_index, options)) {
      continue;
    }

    const auto resolved_mode = resolve_sprite_placement_mode(asset_name, decoded.sprite_index, options.placement_mode);
    const auto destination_rect = compute_sprite_destination_rect(decoded, resolved_mode);
    result.debug_entries.push_back({
        .sprite_index = decoded.sprite_index,
        .width = decoded.sprite.width,
        .height = decoded.sprite.height,
        .descriptor_x = decoded.sprite.x,
        .descriptor_y = decoded.sprite.y,
        .tile_type = decoded.sprite.tile_type,
        .resolved_mode = resolved_mode,
        .destination_rect = destination_rect,
    });

    const auto sprite_w = destination_rect.width;
    const auto sprite_h = destination_rect.height;
    if (sprite_w <= 0 || sprite_h <= 0) {
      ++result.stats.skipped_out_of_bounds_sprites;
      continue;
    }

    const auto src_x0 = destination_rect.x;
    const auto src_y0 = destination_rect.y;
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
