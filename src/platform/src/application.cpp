#include "romulus/platform/application.h"

#include <SDL.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

#include "romulus/core/fixed_timestep_clock.h"
#include "romulus/core/logger.h"
#include "romulus/data/data_root.h"
#include "romulus/platform/viewer.h"

namespace romulus::platform {
namespace {
constexpr char kWindowTitle[] = "Caesar II Reimplementation";
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr auto kSmokeTestRuntime = std::chrono::milliseconds(50);
constexpr auto kFixedStep = std::chrono::milliseconds(16);


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

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    romulus::core::log_error(std::string("SDL_Init failed: ") + SDL_GetError());
    return 1;
  }

  int window_width = kWindowWidth;
  int window_height = kWindowHeight;
  if (options_.debug_view_image.has_value()) {
    const auto layout = compute_viewer_window_layout(
        options_.debug_view_image->width, options_.debug_view_image->height, kWindowWidth, kWindowHeight);
    window_width = layout.window_width;
    window_height = layout.window_height;
    viewer_window_width_ = layout.window_width;
    viewer_window_height_ = layout.window_height;
  }

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

  if (options_.debug_view_image.has_value()) {
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
  }

  romulus::core::FixedTimestepClock clock(kFixedStep);
  auto last_frame_time = std::chrono::steady_clock::now();
  const auto started_at = last_frame_time;

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) {
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

  const SDL_Rect destination = {
      .x = 0,
      .y = 0,
      .w = viewer_window_width_,
      .h = viewer_window_height_,
  };
  SDL_RenderCopy(renderer_, viewer_texture_, nullptr, &destination);
  SDL_RenderPresent(renderer_);
}

}  // namespace romulus::platform
