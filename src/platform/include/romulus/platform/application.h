#pragma once

#include <filesystem>

namespace romulus::platform {

struct ApplicationOptions {
  bool smoke_test = false;
  std::filesystem::path data_root = ".";
};

class Application {
 public:
  explicit Application(ApplicationOptions options = {});

  int run();

 private:
  void simulate_step();
  void render_frame(double alpha);

  ApplicationOptions options_;
};

}  // namespace romulus::platform
