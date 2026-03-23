#include <SDL.h>

#include <chrono>
#include <string>
#include <string_view>

#include "romulus/core/logger.h"

namespace {
constexpr char kWindowTitle[] = "Caesar II Reimplementation";
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr auto kSmokeTestRuntime = std::chrono::milliseconds(50);

[[nodiscard]] bool is_smoke_test(int argc, char* argv[]) {
  for (int index = 1; index < argc; ++index) {
    if (std::string_view(argv[index]) == "--smoke-test") {
      return true;
    }
  }

  return false;
}
}  // namespace

int main(int argc, char* argv[]) {
  const bool smoke_test = is_smoke_test(argc, argv);

  romulus::core::log_info("Starting Caesar II Reimplementation.");

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    romulus::core::log_error(std::string("SDL_Init failed: ") + SDL_GetError());
    return 1;
  }

  const Uint32 window_flags = smoke_test ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN;
  SDL_Window* window = SDL_CreateWindow(
      kWindowTitle,
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      kWindowWidth,
      kWindowHeight,
      window_flags);

  if (window == nullptr) {
    romulus::core::log_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    SDL_Quit();
    return 1;
  }

  const auto started_at = std::chrono::steady_clock::now();
  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) {
        running = false;
      }
    }

    if (smoke_test && (std::chrono::steady_clock::now() - started_at) >= kSmokeTestRuntime) {
      running = false;
    }

    SDL_Delay(16);
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
  romulus::core::log_info("Shutdown complete.");
  return 0;
}
