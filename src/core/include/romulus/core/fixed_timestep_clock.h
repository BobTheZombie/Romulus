#pragma once

#include <chrono>
#include <cstddef>

namespace romulus::core {

class FixedTimestepClock {
 public:
  using Duration = std::chrono::nanoseconds;

  struct AdvanceResult {
    std::size_t step_count;
    double alpha;
    Duration frame_delta;
  };

  explicit FixedTimestepClock(
      Duration fixed_step,
      Duration max_frame_delta = std::chrono::milliseconds(250));

  [[nodiscard]] AdvanceResult advance(Duration frame_delta);
  [[nodiscard]] Duration fixed_step() const;
  [[nodiscard]] Duration accumulator() const;

 private:
  Duration fixed_step_;
  Duration max_frame_delta_;
  Duration accumulator_;
};

}  // namespace romulus::core
