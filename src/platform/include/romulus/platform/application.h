#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "romulus/data/indexed_image.h"
#include "romulus/data/pl8_sprite_table_resource.h"
#include "romulus/platform/forum_composition.h"
#include "romulus/platform/startup.h"

struct SDL_Renderer;
struct SDL_Texture;

namespace romulus::platform {

struct ApplicationOptions {
  bool smoke_test = false;
  std::optional<std::filesystem::path> data_root;
  std::optional<romulus::data::RgbaImage> debug_view_image;
  std::optional<std::filesystem::path> startup_image_override;
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
  bool update_viewer_texture(const romulus::data::RgbaImage& image);
  void log_forum_sprite_debug_report() const;
  bool rebuild_forum_debug_image();

  ApplicationOptions options_;
  std::optional<romulus::data::RgbaImage> forum_base_image_;
  std::vector<romulus::data::Pl8DecodedSprite> forum_debug_sprites_;
  SpritePlacementOptions forum_debug_options_;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* viewer_texture_ = nullptr;
  int viewer_texture_width_ = 0;
  int viewer_texture_height_ = 0;
};

}  // namespace romulus::platform
