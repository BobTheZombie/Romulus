#pragma once

#include <filesystem>
#include <optional>

#include "romulus/data/indexed_image.h"
#include "romulus/platform/startup.h"

struct SDL_Renderer;
struct SDL_Texture;

namespace romulus::platform {

struct ApplicationOptions {
  bool smoke_test = false;
  std::optional<std::filesystem::path> data_root;
  std::optional<romulus::data::RgbaImage> debug_view_image;
  FolderPicker folder_picker = pick_data_root_with_native_dialog;
  std::filesystem::path startup_config_path = default_startup_config_path();
};

class Application {
 public:
  explicit Application(ApplicationOptions options = {});

  int run();

 private:
  void simulate_step();
  void render_frame(double alpha);
  bool run_bootstrap_flow();

  ApplicationOptions options_;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* viewer_texture_ = nullptr;
  int viewer_window_width_ = 0;
  int viewer_window_height_ = 0;
};

}  // namespace romulus::platform
