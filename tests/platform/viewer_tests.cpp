#include "romulus/platform/viewer.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int assert_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    return 1;
  }

  return 0;
}

int test_compute_viewer_window_layout_scales_up_small_image() {
  const auto layout = romulus::platform::compute_viewer_window_layout(64, 32, 1280, 720);

  if (assert_true(layout.scale == 20, "64x32 image should scale by 20 inside 1280x720") != 0) {
    return 1;
  }

  if (assert_true(layout.window_width == 1280 && layout.window_height == 640,
                  "scaled size should be image dimensions multiplied by scale") != 0) {
    return 1;
  }

  return 0;
}

int test_compute_viewer_window_layout_clamps_scale_to_one_for_large_image() {
  const auto layout = romulus::platform::compute_viewer_window_layout(2000, 1000, 1280, 720);

  if (assert_true(layout.scale == 1, "large images should clamp scale to one") != 0) {
    return 1;
  }

  if (assert_true(layout.window_width == 2000 && layout.window_height == 1000,
                  "scale-one layout should preserve image dimensions") != 0) {
    return 1;
  }

  return 0;
}

int test_compute_viewer_window_layout_returns_default_for_invalid_inputs() {
  const auto layout = romulus::platform::compute_viewer_window_layout(0, 10, 1280, 720);

  if (assert_true(layout.window_width == 640 && layout.window_height == 480 && layout.scale == 1,
                  "invalid dimensions should fall back to default layout") != 0) {
    return 1;
  }

  return 0;
}

}  // namespace

int main() {
  if (test_compute_viewer_window_layout_scales_up_small_image() != 0) {
    return EXIT_FAILURE;
  }

  if (test_compute_viewer_window_layout_clamps_scale_to_one_for_large_image() != 0) {
    return EXIT_FAILURE;
  }

  if (test_compute_viewer_window_layout_returns_default_for_invalid_inputs() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
