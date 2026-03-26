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
#include "romulus/data/ilbm_image.h"
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
    options_.debug_view_image = load_bootstrap_image(*options_.data_root, options_.startup_image_override, &bootstrap_error);
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
