#include <string_view>

#include "romulus/platform/application.h"

namespace {
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
  romulus::platform::Application app({
      .smoke_test = is_smoke_test(argc, argv),
  });

  return app.run();
}
