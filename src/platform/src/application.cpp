#include "romulus/platform/application.h"

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "romulus/core/fixed_timestep_clock.h"
#include "romulus/core/logger.h"
#include "romulus/data/data_root.h"
#include "romulus/data/file_loader.h"
#include "romulus/data/image256_resource.h"
#include "romulus/data/ilbm_image.h"
#include "romulus/data/pl8_image_resource.h"
#include "romulus/platform/bootstrap.h"
#include "romulus/platform/viewer.h"

namespace romulus::platform {
namespace {
constexpr char kWindowTitle[] = "Caesar II Reimplementation";
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr auto kSmokeTestRuntime = std::chrono::milliseconds(50);
constexpr auto kFixedStep = std::chrono::milliseconds(16);

[[nodiscard]] std::optional<romulus::data::RgbaImage> load_bootstrap_image(
    const std::filesystem::path& data_root,
    const std::optional<std::filesystem::path>& override_path,
    std::string* error_message) {
  const auto selected = select_bootstrap_asset(data_root, override_path);
  if (!selected.has_value()) {
    *error_message =
        "Unable to find a startup ILBM asset under the configured data root. Requested paths (case-insensitive "
        "resolution attempted: yes): data/forum.lbm, data/empire2.lbm, data/fading.lbm.";
    return std::nullopt;
  }

  const auto loaded = romulus::data::load_file_to_memory(selected->absolute_path);
  if (!loaded.ok()) {
    *error_message = "Failed to load bootstrap image requested='" + selected->logical_path.string() +
                     "' (case-insensitive resolution attempted=" +
                     std::string(selected->case_insensitive_resolution_attempted ? "yes" : "no") +
                     ", resolved='" + selected->absolute_path.string() + "'): " + loaded.error->message;
    return std::nullopt;
  }

  const auto parsed = romulus::data::parse_ilbm_image(loaded.value->bytes);
  if (!parsed.ok()) {
    *error_message = "Failed to parse bootstrap image '" + selected->logical_path.string() + "': " +
                     parsed.error->message;
    return std::nullopt;
  }

  const auto rgba = romulus::data::convert_ilbm_to_rgba(parsed.value.value());
  if (!rgba.ok()) {
    *error_message = "Failed to decode bootstrap image '" + selected->logical_path.string() + "': " +
                     rgba.error->message;
    return std::nullopt;
  }

  romulus::core::log_info("Bootstrap image selected: requested='" + selected->logical_path.string() +
                         "', case-insensitive resolution attempted=" +
                         std::string(selected->case_insensitive_resolution_attempted ? "yes" : "no") +
                         ", resolved='" + selected->absolute_path.string() + "'");
  return rgba.value;
}

void blend_rgba_layers(romulus::data::RgbaImage* destination, const romulus::data::RgbaImage& overlay) {
  if (destination == nullptr) {
    return;
  }

  if (destination->width != overlay.width || destination->height != overlay.height) {
    return;
  }

  for (std::size_t i = 0; i + 3 < destination->pixels_rgba.size() && i + 3 < overlay.pixels_rgba.size(); i += 4) {
    if (overlay.pixels_rgba[i + 3] == 0) {
      continue;
    }

    destination->pixels_rgba[i] = overlay.pixels_rgba[i];
    destination->pixels_rgba[i + 1] = overlay.pixels_rgba[i + 1];
    destination->pixels_rgba[i + 2] = overlay.pixels_rgba[i + 2];
    destination->pixels_rgba[i + 3] = overlay.pixels_rgba[i + 3];
  }
}

[[nodiscard]] std::optional<romulus::data::RgbaImage> load_forum_composed_image(const std::filesystem::path& data_root,
                                                                                 std::string* error_message) {
  const auto background = select_forum_background_asset(data_root);
  if (!background.has_value()) {
    *error_message = "Unable to resolve forum background layer: data/forum.lbm.";
    return std::nullopt;
  }

  romulus::core::log_info("Forum compose background resolved: requested='" + background->logical_path.string() +
                          "', resolved='" + background->absolute_path.string() + "'");

  const auto loaded_background = romulus::data::load_file_to_memory(background->absolute_path);
  if (!loaded_background.ok()) {
    *error_message = "Failed to load forum background '" + background->logical_path.string() +
                     "': " + loaded_background.error->message;
    return std::nullopt;
  }
  romulus::core::log_info("Forum compose background loaded: bytes=" +
                          std::to_string(loaded_background.value->bytes.size()));

  const auto parsed_background = romulus::data::parse_ilbm_image(loaded_background.value->bytes);
  if (!parsed_background.ok()) {
    *error_message = "Failed to parse forum background '" + background->logical_path.string() +
                     "': " + parsed_background.error->message;
    return std::nullopt;
  }
  romulus::core::log_info("Forum compose background parsed ILBM.");

  const auto decoded_background = romulus::data::convert_ilbm_to_rgba(parsed_background.value.value());
  if (!decoded_background.ok()) {
    *error_message = "Failed to decode forum background '" + background->logical_path.string() +
                     "': " + decoded_background.error->message;
    return std::nullopt;
  }
  romulus::core::log_info("Forum compose background decoded RGBA " + std::to_string(decoded_background.value->width) +
                          "x" + std::to_string(decoded_background.value->height));

  romulus::data::RgbaImage composed = decoded_background.value.value();
  for (const auto& overlay_spec : default_forum_overlay_specs()) {
    romulus::core::log_info("Forum compose overlay resolving: image='" + overlay_spec.image_pl8_path.string() +
                            "', palette='" + overlay_spec.palette_256_path.string() + "'");
    const auto selection = select_forum_overlay_asset(data_root, overlay_spec);
    if (!selection.has_value()) {
      romulus::core::log_warning("Forum compose overlay skipped: unable to resolve required files for image='" +
                                 overlay_spec.image_pl8_path.string() + "', palette='" +
                                 overlay_spec.palette_256_path.string() + "'");
      continue;
    }

    const auto loaded_image = romulus::data::load_file_to_memory(selection->image_pl8_absolute_path);
    if (!loaded_image.ok()) {
      romulus::core::log_warning("Forum compose overlay skipped: failed to load image='" +
                                 selection->image_pl8_logical_path.string() + "' reason='" +
                                 loaded_image.error->message + "'");
      continue;
    }

    const auto loaded_palette = romulus::data::load_file_to_memory(selection->palette_256_absolute_path);
    if (!loaded_palette.ok()) {
      romulus::core::log_warning("Forum compose overlay skipped: failed to load palette='" +
                                 selection->palette_256_logical_path.string() + "' reason='" +
                                 loaded_palette.error->message + "'");
      continue;
    }

    romulus::core::log_info("Forum compose overlay loaded: image='" + selection->image_pl8_logical_path.string() +
                            "' bytes=" + std::to_string(loaded_image.value->bytes.size()) + ", palette='" +
                            selection->palette_256_logical_path.string() +
                            "' bytes=" + std::to_string(loaded_palette.value->bytes.size()));

    const auto decoded_overlay = romulus::data::decode_caesar2_forum_pl8_image_pair(loaded_image.value->bytes,
                                                                                     loaded_palette.value->bytes,
                                                                                     true);
    if (!decoded_overlay.ok()) {
      romulus::core::log_warning("Forum compose overlay skipped: decode failed for image='" +
                                 selection->image_pl8_logical_path.string() + "' reason='" +
                                 decoded_overlay.error->message + "'");
      continue;
    }

    if (decoded_overlay.value->rgba_image.width != composed.width ||
        decoded_overlay.value->rgba_image.height != composed.height) {
      romulus::core::log_warning("Forum compose overlay skipped: size mismatch image='" +
                                 selection->image_pl8_logical_path.string() + "' decoded=" +
                                 std::to_string(decoded_overlay.value->rgba_image.width) + "x" +
                                 std::to_string(decoded_overlay.value->rgba_image.height) + " background=" +
                                 std::to_string(composed.width) + "x" + std::to_string(composed.height));
      continue;
    }

    blend_rgba_layers(&composed, decoded_overlay.value->rgba_image);
    romulus::core::log_info("Forum compose overlay composed: image='" + selection->image_pl8_logical_path.string() +
                            "', palette='" + selection->palette_256_logical_path.string() + "'");
  }

  return composed;
}

[[nodiscard]] int show_wizard_message_box(const SetupWizardSnapshot& snapshot) {
  SDL_MessageBoxButtonData buttons[3] = {};
  int button_count = 0;

  const auto add_button = [&](const int id, const Uint32 flags, const char* text) {
    buttons[button_count++] = SDL_MessageBoxButtonData{flags, id, text};
  };

  switch (snapshot.state) {
    case SetupWizardState::WizardWelcome:
      add_button(1, SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, "Choose Folder");
      add_button(0, SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, "Exit");
      break;
    case SetupWizardState::WizardChooseFolder:
      add_button(1, SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, "Open Folder Picker");
      add_button(0, SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, "Exit");
      break;
    case SetupWizardState::WizardValidationFailed:
      add_button(1, SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, "Retry");
      add_button(0, SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, "Exit");
      break;
    case SetupWizardState::WizardConfirm:
      add_button(2, SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, "Save and Continue");
      add_button(1, 0, "Choose Different Folder");
      add_button(0, SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, "Exit");
      break;
    case SetupWizardState::WizardDone:
      add_button(1, SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, "Continue");
      break;
    case SetupWizardState::WizardValidating:
      add_button(1, SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, "Continue");
      break;
  }

  const SDL_MessageBoxData data = {
      .flags = SDL_MESSAGEBOX_INFORMATION,
      .window = nullptr,
      .title = "Romulus Setup",
      .message = snapshot.summary.c_str(),
      .numbuttons = button_count,
      .buttons = buttons,
      .colorScheme = nullptr,
  };

  int button = -1;
  if (SDL_ShowMessageBox(&data, &button) != 0) {
    romulus::core::log_error(std::string("SDL_ShowMessageBox failed: ") + SDL_GetError());
    return 0;
  }

  return button;
}

[[nodiscard]] SetupWizardAction message_box_prompt(const SetupWizardSnapshot& snapshot) {
  if (snapshot.state == SetupWizardState::WizardValidating) {
    return SetupWizardAction::Continue;
  }

  const int button = show_wizard_message_box(snapshot);
  switch (snapshot.state) {
    case SetupWizardState::WizardWelcome:
    case SetupWizardState::WizardChooseFolder:
      return button == 1 ? SetupWizardAction::ChooseFolder : SetupWizardAction::Exit;
    case SetupWizardState::WizardValidationFailed:
      return button == 1 ? SetupWizardAction::Retry : SetupWizardAction::Exit;
    case SetupWizardState::WizardConfirm:
      if (button == 2) {
        return SetupWizardAction::Confirm;
      }

      if (button == 1) {
        return SetupWizardAction::Retry;
      }

      return SetupWizardAction::Exit;
    case SetupWizardState::WizardDone:
    case SetupWizardState::WizardValidating:
      return SetupWizardAction::Continue;
  }

  return SetupWizardAction::Exit;
}
}  // namespace

Application::Application(ApplicationOptions options) : options_(std::move(options)) {}

int Application::run() {
  StartupStatus startup = evaluate_startup_data_root(options_.data_root);
  if (startup.state != StartupState::DataRootReady) {
    romulus::core::log_warning(startup.message);
    if (!run_bootstrap_flow()) {
      return 0;
    }

    startup = evaluate_startup_data_root(options_.data_root);
    if (startup.state != StartupState::DataRootReady) {
      romulus::core::log_error(startup.message);
      return 1;
    }
  }

  romulus::core::log_info("Starting Caesar II Reimplementation.");
  romulus::core::log_info(std::string("Using Caesar II data root: ") + options_.data_root->string());

  if (!options_.debug_view_image.has_value()) {
    std::string bootstrap_error;
    options_.debug_view_image = load_forum_composed_image(*options_.data_root, &bootstrap_error);
    if (!options_.debug_view_image.has_value() && options_.startup_image_override.has_value()) {
      romulus::core::log_warning("Forum composition path unavailable; trying override bootstrap image.");
      options_.debug_view_image = load_bootstrap_image(*options_.data_root, options_.startup_image_override, &bootstrap_error);
    }

    if (!options_.debug_view_image.has_value() && !options_.startup_image_override.has_value()) {
      romulus::core::log_warning("Forum composition path unavailable; falling back to bootstrap image candidates.");
      options_.debug_view_image = load_bootstrap_image(*options_.data_root, std::nullopt, &bootstrap_error);
    }

    if (!options_.debug_view_image.has_value()) {
      romulus::core::log_error(bootstrap_error);
      return 1;
    }
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    romulus::core::log_error(std::string("SDL_Init failed: ") + SDL_GetError());
    return 1;
  }

  int window_width = kWindowWidth;
  int window_height = kWindowHeight;

  const Uint32 window_flags = options_.smoke_test ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN;
  SDL_Window* window = SDL_CreateWindow(
      kWindowTitle,
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      window_width,
      window_height,
      window_flags);

  if (window == nullptr) {
    romulus::core::log_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    SDL_Quit();
    return 1;
  }

  const auto& image = options_.debug_view_image.value();
  const std::size_t expected_bytes = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4;
  if (image.pixels_rgba.size() != expected_bytes) {
    romulus::core::log_error("Debug view image has invalid RGBA buffer length.");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  renderer_ = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer_ == nullptr) {
    renderer_ = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }

  if (renderer_ == nullptr) {
    romulus::core::log_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  viewer_texture_ = SDL_CreateTexture(
      renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.width, image.height);
  if (viewer_texture_ == nullptr) {
    romulus::core::log_error(std::string("SDL_CreateTexture failed: ") + SDL_GetError());
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  if (SDL_UpdateTexture(viewer_texture_, nullptr, image.pixels_rgba.data(), static_cast<int>(image.width) * 4) != 0) {
    romulus::core::log_error(std::string("SDL_UpdateTexture failed: ") + SDL_GetError());
    SDL_DestroyTexture(viewer_texture_);
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  viewer_texture_width_ = image.width;
  viewer_texture_height_ = image.height;

  romulus::core::FixedTimestepClock clock(kFixedStep);
  auto last_frame_time = std::chrono::steady_clock::now();
  const auto started_at = last_frame_time;

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        running = false;
      }
    }

    const auto now = std::chrono::steady_clock::now();
    const auto frame_delta =
        std::chrono::duration_cast<romulus::core::FixedTimestepClock::Duration>(now - last_frame_time);
    last_frame_time = now;

    const auto advance = clock.advance(frame_delta);

    for (std::size_t step = 0; step < advance.step_count; ++step) {
      simulate_step();
    }

    render_frame(advance.alpha);

    if (options_.smoke_test && (now - started_at) >= kSmokeTestRuntime) {
      running = false;
    }

    SDL_Delay(1);
  }

  if (viewer_texture_ != nullptr) {
    SDL_DestroyTexture(viewer_texture_);
    viewer_texture_ = nullptr;
  }

  if (renderer_ != nullptr) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
  romulus::core::log_info("Shutdown complete.");

  return 0;
}

bool Application::run_bootstrap_flow() {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    romulus::core::log_error(std::string("SDL_Init failed: ") + SDL_GetError());
    return false;
  }

  const auto startup = evaluate_startup_data_root(options_.data_root);
  const auto result = run_setup_wizard(options_.startup_config_path, options_.folder_picker, message_box_prompt, startup);

  SDL_Quit();

  if (!result.completed || !result.data_root.has_value()) {
    romulus::core::log_warning("Setup wizard exited before completion.");
    return false;
  }

  options_.data_root = result.data_root;
  return true;
}

void Application::simulate_step() {}

void Application::render_frame(double /*alpha*/) {
  if (renderer_ == nullptr || viewer_texture_ == nullptr) {
    return;
  }

  SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
  SDL_RenderClear(renderer_);

  int output_width = kWindowWidth;
  int output_height = kWindowHeight;
  SDL_GetRendererOutputSize(renderer_, &output_width, &output_height);

  const double scale_x = static_cast<double>(output_width) / static_cast<double>(viewer_texture_width_);
  const double scale_y = static_cast<double>(output_height) / static_cast<double>(viewer_texture_height_);
  const double scale = std::min(scale_x, scale_y);
  const int render_width = static_cast<int>(static_cast<double>(viewer_texture_width_) * scale);
  const int render_height = static_cast<int>(static_cast<double>(viewer_texture_height_) * scale);

  const SDL_Rect destination = {
      .x = (output_width - render_width) / 2,
      .y = (output_height - render_height) / 2,
      .w = render_width,
      .h = render_height,
  };
  SDL_RenderCopy(renderer_, viewer_texture_, nullptr, &destination);
  SDL_RenderPresent(renderer_);
}

}  // namespace romulus::platform
