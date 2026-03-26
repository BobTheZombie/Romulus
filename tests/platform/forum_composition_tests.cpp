#include "romulus/platform/forum_composition.h"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

int assert_true(const bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    return 1;
  }
  return 0;
}

romulus::data::Pl8DecodedSprite make_sprite(const std::int16_t x,
                                            const std::int16_t y,
                                            const std::uint16_t width,
                                            const std::uint16_t height,
                                            const std::uint8_t red,
                                            const std::uint8_t green,
                                            const std::uint8_t blue,
                                            const std::vector<bool>& transparent_mask = {}) {
  romulus::data::Pl8DecodedSprite sprite;
  sprite.sprite.x = x;
  sprite.sprite.y = y;
  sprite.sprite.width = width;
  sprite.sprite.height = height;
  sprite.rgba_image.width = width;
  sprite.rgba_image.height = height;
  sprite.rgba_image.pixels_rgba.assign(static_cast<std::size_t>(width) * height * 4U, 0);

  for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i) {
    const bool transparent = i < transparent_mask.size() && transparent_mask[i];
    sprite.rgba_image.pixels_rgba[i * 4U] = red;
    sprite.rgba_image.pixels_rgba[i * 4U + 1] = green;
    sprite.rgba_image.pixels_rgba[i * 4U + 2] = blue;
    sprite.rgba_image.pixels_rgba[i * 4U + 3] = transparent ? 0 : 255;
  }

  return sprite;
}

std::uint8_t pixel_channel(const romulus::data::RgbaImage& image,
                           const std::size_t x,
                           const std::size_t y,
                           const std::size_t channel) {
  return image.pixels_rgba[(y * image.width + x) * 4U + channel];
}

int test_places_sprites_into_larger_canvas() {
  const auto result = romulus::platform::compose_sprite_layer_to_canvas(
      8,
      8,
      {make_sprite(5, 6, 2, 1, 7, 9, 11)});
  if (assert_true(result.has_value(), "placement into valid canvas should succeed") != 0) {
    return 1;
  }

  if (assert_true(result->rgba_image.width == 8 && result->rgba_image.height == 8,
                  "result should preserve full target canvas size") != 0) {
    return 1;
  }

  if (assert_true(pixel_channel(result->rgba_image, 5, 6, 0) == 7 && pixel_channel(result->rgba_image, 6, 6, 1) == 9,
                  "sprite pixels should be copied at descriptor coordinates") != 0) {
    return 1;
  }

  return 0;
}

int test_composition_order_is_deterministic() {
  const auto result = romulus::platform::compose_sprite_layer_to_canvas(
      4,
      4,
      {make_sprite(1, 1, 2, 2, 10, 0, 0), make_sprite(1, 1, 2, 2, 20, 0, 0)});
  if (assert_true(result.has_value(), "placement should succeed") != 0) {
    return 1;
  }

  return assert_true(pixel_channel(result->rgba_image, 1, 1, 0) == 20,
                     "later descriptors should overwrite earlier descriptors at overlapping pixels");
}

int test_transparent_pixels_do_not_overwrite() {
  const auto base = make_sprite(1, 1, 2, 2, 30, 0, 0);
  const auto top = make_sprite(1, 1, 2, 2, 80, 0, 0, {true, false, false, false});
  const auto result = romulus::platform::compose_sprite_layer_to_canvas(4, 4, {base, top});
  if (assert_true(result.has_value(), "placement should succeed") != 0) {
    return 1;
  }

  if (assert_true(pixel_channel(result->rgba_image, 1, 1, 0) == 30,
                  "transparent top pixel should preserve lower pixel") != 0) {
    return 1;
  }

  return assert_true(pixel_channel(result->rgba_image, 2, 1, 0) == 80,
                     "opaque top pixel should overwrite lower pixel");
}

int test_out_of_bounds_policy_clips_and_skips_deterministically() {
  const auto result = romulus::platform::compose_sprite_layer_to_canvas(
      4,
      4,
      {make_sprite(-1, 0, 3, 2, 4, 0, 0), make_sprite(10, 10, 1, 1, 9, 0, 0)});
  if (assert_true(result.has_value(), "placement should succeed") != 0) {
    return 1;
  }

  if (assert_true(result->stats.placed_sprites == 1 && result->stats.clipped_sprites == 1 &&
                      result->stats.skipped_out_of_bounds_sprites == 1,
                  "out-of-bounds policy should clip partial intersections and skip non-intersections") != 0) {
    return 1;
  }

  if (assert_true(pixel_channel(result->rgba_image, 0, 0, 0) == 4 && pixel_channel(result->rgba_image, 1, 0, 0) == 4,
                  "clipped sprite should still draw in-bounds pixels") != 0) {
    return 1;
  }

  return 0;
}

int test_smaller_sprite_bbox_yields_full_canvas_overlay() {
  const auto result = romulus::platform::compose_sprite_layer_to_canvas(
      640,
      480,
      {make_sprite(100, 100, 5, 5, 2, 3, 4)});
  if (assert_true(result.has_value(), "placement should succeed") != 0) {
    return 1;
  }

  if (assert_true(result->rgba_image.width == 640 && result->rgba_image.height == 480,
                  "smaller sprite span should still return full forum-sized canvas") != 0) {
    return 1;
  }

  return assert_true(pixel_channel(result->rgba_image, 104, 104, 2) == 4,
                     "sprite should appear at absolute coordinates on full canvas");
}

int test_destination_rect_calculation_by_mode_is_deterministic() {
  const auto sprite = make_sprite(10, 20, 6, 4, 0, 0, 0);
  const auto top_left = romulus::platform::compute_sprite_destination_rect(
      sprite, romulus::platform::SpritePlacementMode::TopLeft);
  const auto bottom_left = romulus::platform::compute_sprite_destination_rect(
      sprite, romulus::platform::SpritePlacementMode::BottomLeft);
  const auto centered = romulus::platform::compute_sprite_destination_rect(
      sprite, romulus::platform::SpritePlacementMode::Centered);
  const auto top_center = romulus::platform::compute_sprite_destination_rect(
      sprite, romulus::platform::SpritePlacementMode::TopCenter);
  const auto bottom_center = romulus::platform::compute_sprite_destination_rect(
      sprite, romulus::platform::SpritePlacementMode::BottomCenter);

  if (assert_true(top_left.x == 10 && top_left.y == 20, "top-left placement should preserve descriptor anchor") != 0) {
    return 1;
  }
  if (assert_true(bottom_left.x == 10 && bottom_left.y == 16, "bottom-left placement should offset by height") != 0) {
    return 1;
  }
  if (assert_true(centered.x == 7 && centered.y == 18, "centered placement should subtract half width/height") != 0) {
    return 1;
  }
  if (assert_true(top_center.x == 7 && top_center.y == 20,
                  "top-center placement should subtract half width while preserving descriptor y") != 0) {
    return 1;
  }
  return assert_true(bottom_center.x == 7 && bottom_center.y == 16,
                     "bottom-center placement should subtract half width and full height");
}

int test_asset_specific_mapping_for_rat_back_is_deterministic() {
  if (assert_true(romulus::platform::resolve_sprite_placement_mode(
                      "RAT_BACK.PL8", 0, romulus::platform::SpritePlacementMode::TopLeft) ==
                      romulus::platform::SpritePlacementMode::TopCenter,
                  "RAT_BACK sprite[0] should use top-center placement") != 0) {
    return 1;
  }
  if (assert_true(romulus::platform::resolve_sprite_placement_mode(
                      "rat_back.pl8", 1, romulus::platform::SpritePlacementMode::TopLeft) ==
                      romulus::platform::SpritePlacementMode::BottomCenter,
                  "RAT_BACK sprite[1] should use bottom-center placement") != 0) {
    return 1;
  }
  return assert_true(romulus::platform::resolve_sprite_placement_mode(
                         "assets/rat_back.pl8", 2, romulus::platform::SpritePlacementMode::Centered) ==
                         romulus::platform::SpritePlacementMode::BottomCenter,
                     "RAT_BACK sprite[2] should use bottom-center placement regardless of fallback");
}

int test_asset_specific_mapping_fallback_is_unchanged_for_non_rat_back() {
  return assert_true(romulus::platform::resolve_sprite_placement_mode(
                         "forum.pl8", 0, romulus::platform::SpritePlacementMode::Centered) ==
                         romulus::platform::SpritePlacementMode::Centered,
                     "non-RAT_BACK assets should preserve configured fallback placement mode");
}

int test_sprite_isolation_selection_behavior() {
  romulus::platform::SpritePlacementOptions options;
  if (assert_true(romulus::platform::sprite_is_visible_for_options(0, options),
                  "all sprites should be visible when no isolation is configured") != 0) {
    return 1;
  }

  options.isolated_sprite_index = 2;
  if (assert_true(!romulus::platform::sprite_is_visible_for_options(1, options),
                  "non-selected sprites should be hidden in isolate mode") != 0) {
    return 1;
  }
  return assert_true(romulus::platform::sprite_is_visible_for_options(2, options),
                     "selected sprite should remain visible in isolate mode");
}

int test_placement_report_formatting_is_stable() {
  const auto sprite = make_sprite(3, 4, 5, 6, 0, 0, 0);
  romulus::platform::SpritePlacementOptions options{
      .placement_mode = romulus::platform::SpritePlacementMode::TopLeft,
      .draw_order = romulus::platform::SpriteDrawOrder::Forward,
      .isolated_sprite_index = std::nullopt,
  };
  std::vector<romulus::platform::SpritePlacementDebugEntry> entries{
      {.sprite_index = sprite.sprite_index,
       .width = sprite.sprite.width,
       .height = sprite.sprite.height,
       .descriptor_x = sprite.sprite.x,
       .descriptor_y = sprite.sprite.y,
       .tile_type = sprite.sprite.tile_type,
       .destination_rect = romulus::platform::compute_sprite_destination_rect(
           sprite, romulus::platform::SpritePlacementMode::TopLeft)}};

  const auto report = romulus::platform::format_sprite_placement_report({sprite}, options, entries);
  if (assert_true(report.find("sprite_placement_report: mode=top_left draw_order=forward isolate=all sprite_count=1") !=
                      std::string::npos,
                  "header should include deterministic placement metadata") != 0) {
    return 1;
  }
  return assert_true(report.find("sprite[0]: w=5 h=6 x=3 y=4 tile_type=0 mode=top_left dest=(3,4,5x6)") !=
                         std::string::npos,
                     "entry should include deterministic dimensions, descriptor, resolved mode, and destination rect");
}

int test_bounds_clipping_under_bottom_left_rule() {
  romulus::platform::SpritePlacementOptions options{
      .placement_mode = romulus::platform::SpritePlacementMode::BottomLeft,
      .draw_order = romulus::platform::SpriteDrawOrder::Forward,
      .isolated_sprite_index = std::nullopt,
  };
  const auto result = romulus::platform::compose_sprite_layer_to_canvas(
      4, 4, {make_sprite(0, 1, 2, 2, 55, 0, 0)}, options);
  if (assert_true(result.has_value(), "placement should succeed under bottom-left rule") != 0) {
    return 1;
  }
  if (assert_true(result->stats.placed_sprites == 1 && result->stats.clipped_sprites == 1,
                  "bottom-left rule should clip when transformed y crosses top bound") != 0) {
    return 1;
  }
  return assert_true(pixel_channel(result->rgba_image, 0, 0, 0) == 55,
                     "clipped bottom-left placement should retain in-bounds pixels");
}

}  // namespace

int main() {
  if (test_places_sprites_into_larger_canvas() != 0) {
    return EXIT_FAILURE;
  }

  if (test_composition_order_is_deterministic() != 0) {
    return EXIT_FAILURE;
  }

  if (test_transparent_pixels_do_not_overwrite() != 0) {
    return EXIT_FAILURE;
  }

  if (test_out_of_bounds_policy_clips_and_skips_deterministically() != 0) {
    return EXIT_FAILURE;
  }

  if (test_smaller_sprite_bbox_yields_full_canvas_overlay() != 0) {
    return EXIT_FAILURE;
  }
  if (test_destination_rect_calculation_by_mode_is_deterministic() != 0) {
    return EXIT_FAILURE;
  }
  if (test_asset_specific_mapping_for_rat_back_is_deterministic() != 0) {
    return EXIT_FAILURE;
  }
  if (test_asset_specific_mapping_fallback_is_unchanged_for_non_rat_back() != 0) {
    return EXIT_FAILURE;
  }
  if (test_sprite_isolation_selection_behavior() != 0) {
    return EXIT_FAILURE;
  }
  if (test_placement_report_formatting_is_stable() != 0) {
    return EXIT_FAILURE;
  }
  if (test_bounds_clipping_under_bottom_left_rule() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
