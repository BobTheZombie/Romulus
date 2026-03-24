#pragma once

#include <cstdint>

namespace romulus::platform {

struct ViewerWindowLayout {
  int window_width = 640;
  int window_height = 480;
  int scale = 1;
};

[[nodiscard]] ViewerWindowLayout compute_viewer_window_layout(std::uint16_t image_width,
                                                              std::uint16_t image_height,
                                                              int max_window_width = 1280,
                                                              int max_window_height = 720);

}  // namespace romulus::platform
