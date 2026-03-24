#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "romulus/core/logger.h"
#include "romulus/data/data_root.h"
#include "romulus/platform/application.h"

namespace {

struct ParsedArguments {
  bool smoke_test = false;
  std::optional<std::string> data_dir;
};

[[nodiscard]] std::optional<ParsedArguments> parse_arguments(int argc, char* argv[]) {
  ParsedArguments parsed;

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);

    if (argument == "--smoke-test") {
      parsed.smoke_test = true;
      continue;
    }

    if (argument == "--data-dir") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --data-dir.");
        return std::nullopt;
      }

      parsed.data_dir = argv[++index];
      continue;
    }

    romulus::core::log_error(std::string("Unknown argument: ") + std::string(argument));
    return std::nullopt;
  }

  return parsed;
}

}  // namespace

int main(int argc, char* argv[]) {
  const auto parsed = parse_arguments(argc, argv);
  if (!parsed.has_value()) {
    romulus::core::log_error("Usage: caesar2 [--smoke-test] [--data-dir <path>]");
    return 1;
  }

  const std::filesystem::path data_root = parsed->data_dir.has_value()
                                              ? romulus::data::resolve_data_root(parsed->data_dir.value())
                                              : romulus::data::resolve_data_root(".");

  romulus::platform::Application app({
      .smoke_test = parsed->smoke_test,
      .data_root = data_root,
  });

  return app.run();
}
