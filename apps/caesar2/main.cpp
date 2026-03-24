#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include "romulus/core/logger.h"
#include "romulus/data/binary_probe.h"
#include "romulus/data/data_root.h"
#include "romulus/data/file_loader.h"
#include "romulus/data/file_inventory.h"
#include "romulus/platform/application.h"

namespace {

struct ParsedArguments {
  bool smoke_test = false;
  bool inventory_manifest = false;
  std::optional<std::string> inventory_manifest_out;
  std::optional<std::string> probe_file;
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

    if (argument == "--inventory-manifest") {
      parsed.inventory_manifest = true;
      continue;
    }

    if (argument == "--manifest-out") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --manifest-out.");
        return std::nullopt;
      }

      parsed.inventory_manifest_out = argv[++index];
      continue;
    }

    if (argument == "--probe-file") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --probe-file.");
        return std::nullopt;
      }

      parsed.probe_file = argv[++index];
      continue;
    }

    romulus::core::log_error(std::string("Unknown argument: ") + std::string(argument));
    return std::nullopt;
  }

  if (parsed.inventory_manifest_out.has_value() && !parsed.inventory_manifest) {
    romulus::core::log_error("--manifest-out requires --inventory-manifest.");
    return std::nullopt;
  }

  return parsed;
}

int run_binary_probe(const std::filesystem::path& data_root, const std::string& probe_file_arg) {
  std::filesystem::path candidate = probe_file_arg;
  if (candidate.is_relative()) {
    candidate = data_root / candidate;
  }

  const auto loaded = romulus::data::load_file_to_memory(candidate);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error.value().message);
    return 1;
  }

  const auto report = romulus::data::probe_loaded_binary(loaded.value.value());
  std::cout << romulus::data::format_binary_probe_report(report);
  return 0;
}

int run_manifest_generation(const std::filesystem::path& data_root, const std::optional<std::string>& output_path) {
  const auto validation = romulus::data::validate_data_root(data_root);
  if (!validation.ok) {
    romulus::core::log_error(romulus::data::format_validation_error(validation));
    return 1;
  }

  const auto manifest = romulus::data::build_file_inventory(data_root);
  const auto manifest_text = romulus::data::format_file_inventory_manifest(manifest);

  if (!output_path.has_value()) {
    std::cout << manifest_text;
    return 0;
  }

  std::ofstream stream(*output_path);
  if (!stream.is_open()) {
    romulus::core::log_error("Failed to open manifest output path: " + *output_path);
    return 1;
  }

  stream << manifest_text;
  stream.close();

  if (!stream.good()) {
    romulus::core::log_error("Failed to write manifest output path: " + *output_path);
    return 1;
  }

  romulus::core::log_info("Wrote manifest to: " + *output_path);
  return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
  const auto parsed = parse_arguments(argc, argv);
  if (!parsed.has_value()) {
    romulus::core::log_error(
        "Usage: caesar2 [--smoke-test] [--data-dir <path>] [--inventory-manifest] [--manifest-out <path>] "
        "[--probe-file <path>]");
    return 1;
  }

  const std::filesystem::path data_root = parsed->data_dir.has_value()
                                              ? romulus::data::resolve_data_root(parsed->data_dir.value())
                                              : romulus::data::resolve_data_root(".");

  if (parsed->inventory_manifest) {
    return run_manifest_generation(data_root, parsed->inventory_manifest_out);
  }

  if (parsed->probe_file.has_value()) {
    return run_binary_probe(data_root, parsed->probe_file.value());
  }

  romulus::platform::Application app({
      .smoke_test = parsed->smoke_test,
      .data_root = data_root,
  });

  return app.run();
}
