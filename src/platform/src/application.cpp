#include "romulus/platform/application.h"

#include <SDL.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
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

  SDL_Window* window = SDL_CreateWindow(
      "Romulus Setup",
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      800,
      220,
      options_.smoke_test ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN);

  if (window == nullptr) {
    romulus::core::log_error(std::string("SDL_CreateWindow failed in setup: ") + SDL_GetError());
    SDL_Quit();
    return false;
  }

  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                           "Romulus Setup",
                           "No valid Caesar II Win95 data root is configured.\n"
                           "Press O to choose a folder, or Esc to exit.",
                           window);

  bool running = true;
  bool ready = false;
  while (running && !ready) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) {
        running = false;
      }

      if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_ESCAPE) {
          running = false;
        }

        if (event.key.keysym.sym == SDLK_o) {
          const auto selected = options_.folder_picker();
          if (!selected.has_value()) {
            romulus::core::log_warning("Folder selection canceled or unavailable.");
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
                                     "Romulus Setup",
                                     "No folder was selected.\nPress O to try again, or Esc to exit.",
                                     window);
            continue;
          }

          const auto resolved = romulus::data::resolve_data_root(selected->string());
          const auto status = evaluate_startup_data_root(std::optional<std::filesystem::path>(resolved));
          if (status.state != StartupState::DataRootReady) {
            romulus::core::log_warning(status.message);
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Invalid Caesar II folder", status.message.c_str(), window);
            continue;
          }

          options_.data_root = resolved;
          if (!persist_data_root(options_.startup_config_path, resolved)) {
            romulus::core::log_warning("Selected data root is valid but could not be persisted.");
          }

          ready = true;
        }
      }
    }

    SDL_Delay(1);
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
  return ready;
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
