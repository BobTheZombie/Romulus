#pragma once

#include <filesystem>
#include <optional>

#include "romulus/data/indexed_image.h"

struct SDL_Renderer;
struct SDL_Texture;

namespace romulus::platform {

struct ApplicationOptions {
  bool smoke_test = false;
  std::filesystem::path data_root = ".";
  std::optional<romulus::data::RgbaImage> debug_view_image;
};

class Application {
 public:
  explicit Application(ApplicationOptions options = {});

  int run();

 private:
  void simulate_step();
  void render_frame(double alpha);

  ApplicationOptions options_;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* viewer_texture_ = nullptr;
  int viewer_window_width_ = 0;
  int viewer_window_height_ = 0;
};

}  // namespace romulus::platform
