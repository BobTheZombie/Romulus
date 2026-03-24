#include "romulus/core/fixed_timestep_clock.h"

#include <chrono>
#include <cstdlib>
#include <iostream>

namespace {

int assert_true(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    return 1;
  }

  return 0;
}

}  // namespace

int main() {
  using namespace std::chrono_literals;
  using romulus::core::FixedTimestepClock;

  {
    FixedTimestepClock clock(16ms);
    const auto result = clock.advance(16ms);
    if (assert_true(result.step_count == 1, "16ms should advance exactly one fixed step") != 0) {
      return EXIT_FAILURE;
    }
    if (assert_true(result.alpha == 0.0, "alpha should be zero after an exact step") != 0) {
      return EXIT_FAILURE;
    }
  }

  {
    FixedTimestepClock clock(16ms);
    const auto first = clock.advance(20ms);
    if (assert_true(first.step_count == 1, "20ms should produce one step") != 0) {
      return EXIT_FAILURE;
    }
    if (assert_true(first.alpha > 0.0 && first.alpha < 1.0, "alpha should represent leftover accumulator") != 0) {
      return EXIT_FAILURE;
    }

    const auto second = clock.advance(12ms);
    if (assert_true(second.step_count == 1, "leftover accumulator should carry into the next frame") != 0) {
      return EXIT_FAILURE;
    }
  }

  {
    FixedTimestepClock clock(16ms);
    const auto result = clock.advance(1s);
    if (assert_true(result.frame_delta == 250ms, "frame delta should clamp to 250ms") != 0) {
      return EXIT_FAILURE;
    }
    if (assert_true(result.step_count == 15, "250ms clamp should produce 15 fixed steps at 16ms") != 0) {
      return EXIT_FAILURE;
    }
  }

  {
    FixedTimestepClock clock(16ms);
    const auto result = clock.advance(-1ms);
    if (assert_true(result.step_count == 0, "negative frame times should be clamped to zero") != 0) {
      return EXIT_FAILURE;
    }
    if (assert_true(result.frame_delta == 0ms, "negative frame delta should become zero") != 0) {
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
