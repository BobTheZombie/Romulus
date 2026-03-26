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

  return EXIT_SUCCESS;
}
