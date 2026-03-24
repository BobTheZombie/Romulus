#include "romulus/platform/viewer.h"

#include <algorithm>

namespace romulus::platform {

ViewerWindowLayout compute_viewer_window_layout(const std::uint16_t image_width,
                                                const std::uint16_t image_height,
                                                const int max_window_width,
                                                const int max_window_height) {
  if (image_width == 0 || image_height == 0 || max_window_width <= 0 || max_window_height <= 0) {
    return {};
  }

  const int width = static_cast<int>(image_width);
  const int height = static_cast<int>(image_height);

  const int max_x_scale = std::max(1, max_window_width / width);
  const int max_y_scale = std::max(1, max_window_height / height);
  const int scale = std::max(1, std::min(max_x_scale, max_y_scale));

  return {
      .window_width = width * scale,
      .window_height = height * scale,
      .scale = scale,
  };
}

}  // namespace romulus::platform
