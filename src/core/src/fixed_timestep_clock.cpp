#include "romulus/core/fixed_timestep_clock.h"

namespace romulus::core {

FixedTimestepClock::FixedTimestepClock(Duration fixed_step, Duration max_frame_delta)
    : fixed_step_(fixed_step > Duration::zero() ? fixed_step : Duration(1)),
      max_frame_delta_(max_frame_delta > Duration::zero() ? max_frame_delta : Duration(1)),
      accumulator_(Duration::zero()) {}

FixedTimestepClock::AdvanceResult FixedTimestepClock::advance(Duration frame_delta) {
  if (frame_delta < Duration::zero()) {
    frame_delta = Duration::zero();
  }

  if (frame_delta > max_frame_delta_) {
    frame_delta = max_frame_delta_;
  }

  accumulator_ += frame_delta;

  std::size_t step_count = 0;
  while (accumulator_ >= fixed_step_) {
    accumulator_ -= fixed_step_;
    ++step_count;
  }

  const double alpha = static_cast<double>(accumulator_.count()) /
                       static_cast<double>(fixed_step_.count());

  return {
      .step_count = step_count,
      .alpha = alpha,
      .frame_delta = frame_delta,
  };
}

FixedTimestepClock::Duration FixedTimestepClock::fixed_step() const {
  return fixed_step_;
}

FixedTimestepClock::Duration FixedTimestepClock::accumulator() const {
  return accumulator_;
}

}  // namespace romulus::core
