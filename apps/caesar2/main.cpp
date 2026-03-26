#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "romulus/core/logger.h"
#include "romulus/data/binary_probe.h"
#include "romulus/data/candidate_probe.h"
#include "romulus/data/data_root.h"
#include "romulus/data/signature_registry.h"
#include "romulus/data/file_inventory.h"
#include "romulus/data/file_loader.h"
#include "romulus/data/image_export.h"
#include "romulus/data/indexed_image.h"
#include "romulus/data/palette.h"
#include "romulus/data/ilbm_image.h"
#include "romulus/data/pl8_resource.h"
#include "romulus/data/pe_exe_resource.h"
#include "romulus/data/win95_data_container.h"
#include "romulus/data/win95_data_probe.h"
#include "romulus/platform/application.h"
#include "romulus/platform/startup.h"

namespace {

struct ParsedArguments {
  bool smoke_test = false;
  bool inventory_manifest = false;
  std::optional<std::string> inventory_manifest_out;
  std::optional<std::string> probe_file;
  std::vector<std::string> probe_candidates;
  std::optional<std::string> match_signature;
  std::vector<std::string> classify_candidates;
  bool classify_include_secondary = false;
  std::optional<std::string> export_tile_file;
  std::optional<std::string> export_palette_file;
  std::optional<std::string> export_output_file;
  std::optional<std::string> view_tile_file;
  std::optional<std::string> view_palette_file;
  std::optional<std::string> probe_lbm_file;
  std::vector<std::string> probe_pl8_files;
  std::optional<std::string> probe_exe_file;
  std::optional<std::string> probe_exe_resources_file;
  std::optional<std::string> probe_exe_resource_payloads_file;
  std::optional<std::string> export_lbm_file;
  std::optional<std::string> view_lbm_file;
  bool probe_win95_data = false;
  std::optional<std::string> probe_win95_container_file;
  bool probe_win95_container_entries_all = false;
  std::optional<std::string> probe_pack_ilbm_container_file;
  std::optional<std::size_t> probe_pack_ilbm_entry_index;
  std::optional<std::string> probe_pack_ilbm_batch_container_file;
  bool probe_pack_ilbm_batch_entries_all = false;
  std::optional<std::string> extract_pack_ilbm_container_file;
  std::optional<std::size_t> extract_pack_ilbm_entry_index;
  std::optional<std::string> view_pack_ilbm_container_file;
  std::optional<std::size_t> view_pack_ilbm_entry_index;
  bool index_zero_transparent = false;
  std::optional<std::string> data_dir;
};

[[nodiscard]] std::optional<std::size_t> parse_size_t_argument(std::string_view text) {
  std::size_t value = 0;
  const auto* begin = text.data();
  const auto* end = begin + text.size();
  const auto parse = std::from_chars(begin, end, value);
  if (parse.ec != std::errc() || parse.ptr != end) {
    return std::nullopt;
  }

  return value;
}

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

    if (argument == "--probe-candidate") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --probe-candidate.");
        return std::nullopt;
      }

      parsed.probe_candidates.emplace_back(argv[++index]);
      continue;
    }

    if (argument == "--match-signature") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --match-signature.");
        return std::nullopt;
      }

      parsed.match_signature = argv[++index];
      continue;
    }

    if (argument == "--classify-candidate") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --classify-candidate.");
        return std::nullopt;
      }

      parsed.classify_candidates.emplace_back(argv[++index]);
      continue;
    }

    if (argument == "--classify-include-secondary") {
      parsed.classify_include_secondary = true;
      continue;
    }

    if (argument == "--export-tile-file") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --export-tile-file.");
        return std::nullopt;
      }

      parsed.export_tile_file = argv[++index];
      continue;
    }

    if (argument == "--export-palette-file") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --export-palette-file.");
        return std::nullopt;
      }

      parsed.export_palette_file = argv[++index];
      continue;
    }

    if (argument == "--export-output") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --export-output.");
        return std::nullopt;
      }

      parsed.export_output_file = argv[++index];
      continue;
    }

    if (argument == "--view-tile-file") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --view-tile-file.");
        return std::nullopt;
      }

      parsed.view_tile_file = argv[++index];
      continue;
    }

    if (argument == "--view-palette-file") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --view-palette-file.");
        return std::nullopt;
      }

      parsed.view_palette_file = argv[++index];
      continue;
    }

    if (argument == "--index-zero-transparent") {
      parsed.index_zero_transparent = true;
      continue;
    }

    if (argument == "--probe-lbm") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --probe-lbm.");
        return std::nullopt;
      }

      parsed.probe_lbm_file = argv[++index];
      continue;
    }

    if (argument == "--probe-pl8") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --probe-pl8.");
        return std::nullopt;
      }

      parsed.probe_pl8_files.emplace_back(argv[++index]);
      continue;
    }

    if (argument == "--probe-exe") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --probe-exe.");
        return std::nullopt;
      }

      parsed.probe_exe_file = argv[++index];
      continue;
    }

    if (argument == "--probe-exe-resources") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --probe-exe-resources.");
        return std::nullopt;
      }

      parsed.probe_exe_resources_file = argv[++index];
      continue;
    }

    if (argument == "--probe-exe-resource-payloads") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --probe-exe-resource-payloads.");
        return std::nullopt;
      }

      parsed.probe_exe_resource_payloads_file = argv[++index];
      continue;
    }

    if (argument == "--export-lbm-file") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --export-lbm-file.");
        return std::nullopt;
      }

      parsed.export_lbm_file = argv[++index];
      continue;
    }

    if (argument == "--view-lbm-file") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --view-lbm-file.");
        return std::nullopt;
      }

      parsed.view_lbm_file = argv[++index];
      continue;
    }

    if (argument == "--probe-win95-data") {
      parsed.probe_win95_data = true;
      continue;
    }

    if (argument == "--probe-win95-container") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --probe-win95-container.");
        return std::nullopt;
      }

      parsed.probe_win95_container_file = argv[++index];
      continue;
    }

    if (argument == "--probe-win95-container-entries-all") {
      parsed.probe_win95_container_entries_all = true;
      continue;
    }

    if (argument == "--probe-pack-ilbm") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--probe-pack-ilbm requires <container> <entry_index>.");
        return std::nullopt;
      }

      parsed.probe_pack_ilbm_container_file = argv[++index];
      const auto parsed_index = parse_size_t_argument(argv[++index]);
      if (!parsed_index.has_value()) {
        romulus::core::log_error("Invalid --probe-pack-ilbm entry_index; expected a non-negative integer.");
        return std::nullopt;
      }

      parsed.probe_pack_ilbm_entry_index = parsed_index;
      continue;
    }

    if (argument == "--probe-pack-ilbm-batch") {
      if (index + 1 >= argc) {
        romulus::core::log_error("--probe-pack-ilbm-batch requires <container>.");
        return std::nullopt;
      }

      parsed.probe_pack_ilbm_batch_container_file = argv[++index];
      continue;
    }

    if (argument == "--probe-pack-ilbm-batch-entries-all") {
      parsed.probe_pack_ilbm_batch_entries_all = true;
      continue;
    }

    if (argument == "--extract-pack-ilbm") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--extract-pack-ilbm requires <container> <entry_index>.");
        return std::nullopt;
      }

      parsed.extract_pack_ilbm_container_file = argv[++index];
      const auto parsed_index = parse_size_t_argument(argv[++index]);
      if (!parsed_index.has_value()) {
        romulus::core::log_error("Invalid --extract-pack-ilbm entry_index; expected a non-negative integer.");
        return std::nullopt;
      }

      parsed.extract_pack_ilbm_entry_index = parsed_index;
      continue;
    }

    if (argument == "--view-pack-ilbm") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--view-pack-ilbm requires <container> <entry_index>.");
        return std::nullopt;
      }

      parsed.view_pack_ilbm_container_file = argv[++index];
      const auto parsed_index = parse_size_t_argument(argv[++index]);
      if (!parsed_index.has_value()) {
        romulus::core::log_error("Invalid --view-pack-ilbm entry_index; expected a non-negative integer.");
        return std::nullopt;
      }

      parsed.view_pack_ilbm_entry_index = parsed_index;
      continue;
    }

    romulus::core::log_error(std::string("Unknown argument: ") + std::string(argument));
    return std::nullopt;
  }

  if (parsed.inventory_manifest_out.has_value() && !parsed.inventory_manifest) {
    romulus::core::log_error("--manifest-out requires --inventory-manifest.");
    return std::nullopt;
  }

  const bool has_any_tile_export_arg = parsed.export_tile_file.has_value() || parsed.export_palette_file.has_value();
  if (has_any_tile_export_arg) {
    if (!parsed.export_tile_file.has_value() || !parsed.export_palette_file.has_value() ||
        !parsed.export_output_file.has_value()) {
      romulus::core::log_error(
          "Image export requires --export-tile-file, --export-palette-file, and --export-output.");
      return std::nullopt;
    }
  }

  const bool has_any_view_arg = parsed.view_tile_file.has_value() || parsed.view_palette_file.has_value();
  if (has_any_view_arg) {
    if (!parsed.view_tile_file.has_value() || !parsed.view_palette_file.has_value()) {
      romulus::core::log_error("Image viewer requires --view-tile-file and --view-palette-file.");
      return std::nullopt;
    }
  }

  if (has_any_tile_export_arg && has_any_view_arg) {
    romulus::core::log_error("Image export and image viewer modes are mutually exclusive.");
    return std::nullopt;
  }

  const bool has_lbm_export_arg =
      parsed.export_lbm_file.has_value() || (parsed.export_output_file.has_value() && !has_any_tile_export_arg);
  if (has_lbm_export_arg) {
    if (!parsed.export_lbm_file.has_value() || !parsed.export_output_file.has_value()) {
      romulus::core::log_error("LBM export requires --export-lbm-file and --export-output.");
      return std::nullopt;
    }
  }

  const bool has_lbm_view_arg = parsed.view_lbm_file.has_value();
  if (has_lbm_view_arg && parsed.view_tile_file.has_value()) {
    romulus::core::log_error("--view-lbm-file and --view-tile-file are mutually exclusive.");
    return std::nullopt;
  }

  if (has_lbm_export_arg && (has_any_tile_export_arg || has_any_view_arg || has_lbm_view_arg)) {
    romulus::core::log_error("LBM export mode is mutually exclusive with tile export/view and LBM viewer modes.");
    return std::nullopt;
  }

  if (has_lbm_view_arg && (has_any_tile_export_arg || has_any_view_arg || has_lbm_export_arg)) {
    romulus::core::log_error("LBM viewer mode is mutually exclusive with tile export/view and LBM export modes.");
    return std::nullopt;
  }

  if (parsed.probe_file.has_value() && !parsed.probe_candidates.empty()) {
    romulus::core::log_error("--probe-file and --probe-candidate are mutually exclusive.");
    return std::nullopt;
  }

  if (parsed.match_signature.has_value() && parsed.probe_file.has_value()) {
    romulus::core::log_error("--match-signature and --probe-file are mutually exclusive.");
    return std::nullopt;
  }

  if (parsed.match_signature.has_value() && !parsed.probe_candidates.empty()) {
    romulus::core::log_error("--match-signature and --probe-candidate are mutually exclusive.");
    return std::nullopt;
  }

  if (parsed.match_signature.has_value() && !parsed.classify_candidates.empty()) {
    romulus::core::log_error("--match-signature and --classify-candidate are mutually exclusive.");
    return std::nullopt;
  }

  if (!parsed.probe_candidates.empty() && !parsed.classify_candidates.empty()) {
    romulus::core::log_error("--probe-candidate and --classify-candidate are mutually exclusive.");
    return std::nullopt;
  }

  if (parsed.classify_include_secondary && parsed.classify_candidates.empty()) {
    romulus::core::log_error("--classify-include-secondary requires --classify-candidate.");
    return std::nullopt;
  }

  if (parsed.probe_lbm_file.has_value()) {
    const bool has_other_mode = parsed.inventory_manifest || parsed.probe_file.has_value() ||
                                !parsed.probe_candidates.empty() || parsed.match_signature.has_value() ||
                                !parsed.classify_candidates.empty() || parsed.export_tile_file.has_value() ||
                                parsed.view_tile_file.has_value() || parsed.export_lbm_file.has_value() ||
                                parsed.view_lbm_file.has_value();
    if (has_other_mode) {
      romulus::core::log_error("--probe-lbm is mutually exclusive with other command modes.");
      return std::nullopt;
    }
  }

  if (!parsed.probe_pl8_files.empty()) {
    const bool has_other_mode = parsed.inventory_manifest || parsed.probe_file.has_value() ||
                                !parsed.probe_candidates.empty() || parsed.match_signature.has_value() ||
                                !parsed.classify_candidates.empty() || parsed.export_tile_file.has_value() ||
                                parsed.view_tile_file.has_value() || parsed.export_lbm_file.has_value() ||
                                parsed.view_lbm_file.has_value() || parsed.probe_lbm_file.has_value();
    if (has_other_mode) {
      romulus::core::log_error("--probe-pl8 is mutually exclusive with other command modes.");
      return std::nullopt;
    }
  }

  if (parsed.probe_exe_file.has_value()) {
    const bool has_other_mode = parsed.inventory_manifest || parsed.probe_file.has_value() ||
                                !parsed.probe_candidates.empty() || parsed.match_signature.has_value() ||
                                !parsed.classify_candidates.empty() || parsed.export_tile_file.has_value() ||
                                parsed.view_tile_file.has_value() || parsed.export_lbm_file.has_value() ||
                                parsed.view_lbm_file.has_value() || parsed.probe_lbm_file.has_value() ||
                                !parsed.probe_pl8_files.empty();
    if (has_other_mode) {
      romulus::core::log_error("--probe-exe is mutually exclusive with other command modes.");
      return std::nullopt;
    }
  }

  if (parsed.probe_exe_resources_file.has_value()) {
    const bool has_other_mode = parsed.inventory_manifest || parsed.probe_file.has_value() ||
                                !parsed.probe_candidates.empty() || parsed.match_signature.has_value() ||
                                !parsed.classify_candidates.empty() || parsed.export_tile_file.has_value() ||
                                parsed.view_tile_file.has_value() || parsed.export_lbm_file.has_value() ||
                                parsed.view_lbm_file.has_value() || parsed.probe_lbm_file.has_value() ||
                                !parsed.probe_pl8_files.empty() || parsed.probe_exe_file.has_value();
    if (has_other_mode) {
      romulus::core::log_error("--probe-exe-resources is mutually exclusive with other command modes.");
      return std::nullopt;
    }
  }

  if (parsed.probe_exe_resource_payloads_file.has_value()) {
    const bool has_other_mode = parsed.inventory_manifest || parsed.probe_file.has_value() ||
                                !parsed.probe_candidates.empty() || parsed.match_signature.has_value() ||
                                !parsed.classify_candidates.empty() || parsed.export_tile_file.has_value() ||
                                parsed.view_tile_file.has_value() || parsed.export_lbm_file.has_value() ||
                                parsed.view_lbm_file.has_value() || parsed.probe_lbm_file.has_value() ||
                                !parsed.probe_pl8_files.empty() || parsed.probe_exe_file.has_value() ||
                                parsed.probe_exe_resources_file.has_value();
    if (has_other_mode) {
      romulus::core::log_error("--probe-exe-resource-payloads is mutually exclusive with other command modes.");
      return std::nullopt;
    }
  }

  if (parsed.probe_win95_data) {
    const bool has_other_mode = parsed.inventory_manifest || parsed.probe_file.has_value() ||
                                !parsed.probe_candidates.empty() || parsed.match_signature.has_value() ||
                                !parsed.classify_candidates.empty() || parsed.export_tile_file.has_value() ||
                                parsed.view_tile_file.has_value() || parsed.export_lbm_file.has_value() ||
                                parsed.view_lbm_file.has_value() || parsed.probe_lbm_file.has_value() ||
                                !parsed.probe_pl8_files.empty() || parsed.probe_exe_file.has_value() ||
                                parsed.probe_exe_resources_file.has_value() ||
                                parsed.probe_exe_resource_payloads_file.has_value();
    if (has_other_mode) {
      romulus::core::log_error("--probe-win95-data is mutually exclusive with other command modes.");
      return std::nullopt;
    }
  }

  if (parsed.probe_win95_container_file.has_value()) {
    const bool has_other_mode = parsed.inventory_manifest || parsed.probe_file.has_value() ||
                                !parsed.probe_candidates.empty() || parsed.match_signature.has_value() ||
                                !parsed.classify_candidates.empty() || parsed.export_tile_file.has_value() ||
                                parsed.view_tile_file.has_value() || parsed.export_lbm_file.has_value() ||
                                parsed.view_lbm_file.has_value() || parsed.probe_lbm_file.has_value() ||
                                !parsed.probe_pl8_files.empty() || parsed.probe_exe_file.has_value() ||
                                parsed.probe_exe_resources_file.has_value() ||
                                parsed.probe_exe_resource_payloads_file.has_value() || parsed.probe_win95_data;
    if (has_other_mode) {
      romulus::core::log_error("--probe-win95-container is mutually exclusive with other command modes.");
      return std::nullopt;
    }
  }

  if (parsed.probe_win95_container_entries_all && !parsed.probe_win95_container_file.has_value()) {
    romulus::core::log_error("--probe-win95-container-entries-all requires --probe-win95-container.");
    return std::nullopt;
  }

  const bool has_pack_ilbm_probe = parsed.probe_pack_ilbm_container_file.has_value();
  const bool has_pack_ilbm_batch_probe = parsed.probe_pack_ilbm_batch_container_file.has_value();
  const bool has_pack_ilbm_extract = parsed.extract_pack_ilbm_container_file.has_value();
  const bool has_pack_ilbm_view = parsed.view_pack_ilbm_container_file.has_value();
  const int pack_ilbm_mode_count = static_cast<int>(has_pack_ilbm_probe) + static_cast<int>(has_pack_ilbm_batch_probe) +
                                   static_cast<int>(has_pack_ilbm_extract) +
                                   static_cast<int>(has_pack_ilbm_view);
  if (pack_ilbm_mode_count > 1) {
    romulus::core::log_error(
        "--probe-pack-ilbm, --probe-pack-ilbm-batch, --extract-pack-ilbm, and --view-pack-ilbm are mutually exclusive.");
    return std::nullopt;
  }

  if ((has_pack_ilbm_probe && !parsed.probe_pack_ilbm_entry_index.has_value()) ||
      (has_pack_ilbm_extract && !parsed.extract_pack_ilbm_entry_index.has_value()) ||
      (has_pack_ilbm_view && !parsed.view_pack_ilbm_entry_index.has_value())) {
    romulus::core::log_error("PACK ILBM commands require a valid entry index.");
    return std::nullopt;
  }

  if (parsed.probe_pack_ilbm_batch_entries_all && !has_pack_ilbm_batch_probe) {
    romulus::core::log_error("--probe-pack-ilbm-batch-entries-all requires --probe-pack-ilbm-batch.");
    return std::nullopt;
  }

  if (has_pack_ilbm_probe || has_pack_ilbm_batch_probe || has_pack_ilbm_extract || has_pack_ilbm_view) {
    const bool has_other_mode = parsed.inventory_manifest || parsed.probe_file.has_value() ||
                                !parsed.probe_candidates.empty() || parsed.match_signature.has_value() ||
                                !parsed.classify_candidates.empty() || parsed.export_tile_file.has_value() ||
                                parsed.view_tile_file.has_value() || parsed.export_lbm_file.has_value() ||
                                parsed.view_lbm_file.has_value() || parsed.probe_lbm_file.has_value() ||
                                !parsed.probe_pl8_files.empty() || parsed.probe_exe_file.has_value() ||
                                parsed.probe_exe_resources_file.has_value() ||
                                parsed.probe_exe_resource_payloads_file.has_value() || parsed.probe_win95_data ||
                                parsed.probe_win95_container_file.has_value();
    if (has_other_mode) {
      romulus::core::log_error("PACK ILBM commands are mutually exclusive with other command modes.");
      return std::nullopt;
    }
  }

  if (has_pack_ilbm_extract && !parsed.export_output_file.has_value()) {
    romulus::core::log_error("--extract-pack-ilbm requires --export-output.");
    return std::nullopt;
  }

  if (!has_pack_ilbm_extract && parsed.export_output_file.has_value() && !has_any_tile_export_arg && !has_lbm_export_arg) {
    romulus::core::log_error("--export-output requires an export mode.");
    return std::nullopt;
  }

  return parsed;
}

[[nodiscard]] std::filesystem::path resolve_data_relative(const std::filesystem::path& data_root,
                                                          const std::string& path_argument) {
  std::filesystem::path candidate = path_argument;
  if (candidate.is_relative()) {
    candidate = data_root / candidate;
  }

  return candidate;
}

[[nodiscard]] std::optional<romulus::data::RgbaImage> decode_tile_image_to_rgba(const std::filesystem::path& data_root,
                                                                                 const std::string& tile_file_arg,
                                                                                 const std::string& palette_file_arg,
                                                                                 bool index_zero_transparent) {
  const auto tile_path = resolve_data_relative(data_root, tile_file_arg);
  const auto palette_path = resolve_data_relative(data_root, palette_file_arg);

  const auto tile_loaded = romulus::data::load_file_to_memory(tile_path);
  if (!tile_loaded.ok()) {
    romulus::core::log_error(tile_loaded.error.value().message);
    return std::nullopt;
  }

  const auto palette_loaded = romulus::data::load_file_to_memory(palette_path);
  if (!palette_loaded.ok()) {
    romulus::core::log_error(palette_loaded.error.value().message);
    return std::nullopt;
  }

  const auto parsed_tile = romulus::data::parse_caesar2_simple_indexed_tile(tile_loaded.value.value().bytes);
  if (!parsed_tile.ok()) {
    romulus::core::log_error(parsed_tile.error->message);
    return std::nullopt;
  }

  const auto parsed_palette = romulus::data::parse_palette_resource(palette_loaded.value.value().bytes);
  if (!parsed_palette.ok()) {
    romulus::core::log_error(parsed_palette.error->message);
    return std::nullopt;
  }

  const auto rgba_image = romulus::data::apply_palette_to_indexed_image(
      parsed_tile.value.value(), parsed_palette.value.value(), index_zero_transparent);
  if (!rgba_image.ok()) {
    romulus::core::log_error(rgba_image.error->message);
    return std::nullopt;
  }

  return rgba_image.value.value();
}

int run_binary_probe(const std::filesystem::path& data_root, const std::string& probe_file_arg) {
  const auto candidate = resolve_data_relative(data_root, probe_file_arg);

  const auto loaded = romulus::data::load_file_to_memory(candidate);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error.value().message);
    return 1;
  }

  const auto report = romulus::data::probe_loaded_binary(loaded.value.value());
  std::cout << romulus::data::format_binary_probe_report(report);
  return 0;
}

int run_candidate_probe(const std::filesystem::path& data_root, const std::vector<std::string>& candidates) {
  const auto result = romulus::data::probe_candidate_files(data_root, candidates);
  if (!result.ok()) {
    romulus::core::log_error(result.error.value().message);
    return 1;
  }

  std::cout << romulus::data::format_candidate_probe_report(result.value.value());
  return 0;
}

int run_signature_match(const std::filesystem::path& data_root, const std::string& candidate) {
  const auto probe = romulus::data::probe_candidate_files(data_root, {candidate});
  if (!probe.ok()) {
    romulus::core::log_error(probe.error.value().message);
    return 1;
  }

  const auto& bundle = probe.value.value();
  if (bundle.files.empty()) {
    romulus::core::log_error("Signature match expected one probed candidate report.");
    return 1;
  }

  const auto result = romulus::data::match_candidate_signatures(bundle, bundle.files.front());
  std::cout << romulus::data::format_signature_registry_report(result);
  return 0;
}

int run_batch_classification(const std::filesystem::path& data_root,
                             const std::vector<std::string>& candidates,
                             const bool include_secondary_matches) {
  const auto probe = romulus::data::probe_candidate_files(data_root, candidates);
  if (!probe.ok()) {
    romulus::core::log_error(probe.error.value().message);
    return 1;
  }

  const auto report = romulus::data::classify_candidate_batch(probe.value.value(), include_secondary_matches);
  std::cout << romulus::data::format_batch_classification_report(report);
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

int run_tile_export(const std::filesystem::path& data_root,
                    const std::string& tile_file_arg,
                    const std::string& palette_file_arg,
                    const std::string& output_file_arg,
                    bool index_zero_transparent) {
  const auto output_path = resolve_data_relative(data_root, output_file_arg);

  const auto decoded = decode_tile_image_to_rgba(data_root, tile_file_arg, palette_file_arg, index_zero_transparent);
  if (!decoded.has_value()) {
    return 1;
  }

  const auto export_result = romulus::data::export_rgba_image_as_ppm(decoded.value(), output_path);
  if (!export_result.ok()) {
    romulus::core::log_error(export_result.error->message);
    return 1;
  }

  romulus::core::log_info("Exported decoded tile to: " + output_path.string());
  return 0;
}

int run_tile_viewer(const std::filesystem::path& data_root,
                    const std::string& tile_file_arg,
                    const std::string& palette_file_arg,
                    bool smoke_test,
                    bool index_zero_transparent) {
  const auto decoded = decode_tile_image_to_rgba(data_root, tile_file_arg, palette_file_arg, index_zero_transparent);
  if (!decoded.has_value()) {
    return 1;
  }

  romulus::platform::Application app({
      .smoke_test = smoke_test,
      .data_root = data_root,
      .debug_view_image = decoded,
  });

  return app.run();
}

int run_lbm_probe(const std::filesystem::path& data_root, const std::string& lbm_file_arg) {
  const auto candidate = resolve_data_relative(data_root, lbm_file_arg);
  const auto loaded = romulus::data::load_file_to_memory(candidate);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error.value().message);
    return 1;
  }

  const auto parsed = romulus::data::parse_ilbm_image(loaded.value.value().bytes);
  if (!parsed.ok()) {
    romulus::core::log_error(parsed.error.value().message);
    return 1;
  }

  std::cout << romulus::data::format_lbm_report(parsed.value.value());
  return 0;
}

int run_pl8_probe(const std::filesystem::path& data_root, const std::vector<std::string>& pl8_file_args) {
  const auto probed = romulus::data::probe_pl8_files(data_root, pl8_file_args);
  if (!probed.ok()) {
    romulus::core::log_error(probed.error.value().message);
    return 1;
  }

  std::cout << romulus::data::format_pl8_batch_report(probed.value.value());
  return 0;
}

int run_exe_probe(const std::filesystem::path& data_root, const std::string& exe_file_arg) {
  const auto candidate = resolve_data_relative(data_root, exe_file_arg);
  const auto loaded = romulus::data::load_file_to_memory(candidate);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error.value().message);
    return 1;
  }

  const auto parsed = romulus::data::parse_pe_exe_resource(loaded.value.value().bytes);
  if (!parsed.ok()) {
    romulus::core::log_error(parsed.error.value().message);
    return 1;
  }

  const auto payloads = romulus::data::decode_pe_resource_payloads(loaded.value.value().bytes, parsed.value.value());
  if (payloads.ok()) {
    std::cout << romulus::data::format_pe_exe_report(parsed.value.value(), payloads.value.value());
    return 0;
  }

  std::cout << romulus::data::format_pe_exe_report(parsed.value.value());
  return 0;
}

int run_exe_resource_probe(const std::filesystem::path& data_root, const std::string& exe_file_arg) {
  const auto candidate = resolve_data_relative(data_root, exe_file_arg);
  const auto loaded = romulus::data::load_file_to_memory(candidate);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error.value().message);
    return 1;
  }

  const auto parsed = romulus::data::parse_pe_exe_resource(loaded.value.value().bytes);
  if (!parsed.ok()) {
    romulus::core::log_error(parsed.error.value().message);
    return 1;
  }

  std::cout << romulus::data::format_pe_resource_report(parsed.value.value().resource_report);
  return 0;
}

int run_exe_resource_payload_probe(const std::filesystem::path& data_root, const std::string& exe_file_arg) {
  const auto candidate = resolve_data_relative(data_root, exe_file_arg);
  const auto loaded = romulus::data::load_file_to_memory(candidate);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error.value().message);
    return 1;
  }

  const auto parsed = romulus::data::parse_pe_exe_resource(loaded.value.value().bytes);
  if (!parsed.ok()) {
    romulus::core::log_error(parsed.error.value().message);
    return 1;
  }

  const auto payloads = romulus::data::decode_pe_resource_payloads(loaded.value.value().bytes, parsed.value.value());
  if (!payloads.ok()) {
    romulus::core::log_error(payloads.error.value().message);
    return 1;
  }

  std::cout << romulus::data::format_pe_resource_payload_report(payloads.value.value());
  return 0;
}

int run_win95_data_probe(const std::filesystem::path& data_root) {
  const auto result = romulus::data::probe_win95_data_entries(data_root);
  if (!result.ok()) {
    romulus::core::log_error(result.error.value().message);
    return 1;
  }

  std::cout << romulus::data::format_win95_data_probe_report(result.value.value());
  return 0;
}

int run_win95_container_probe(const std::filesystem::path& data_root,
                              const std::string& candidate_arg,
                              const bool include_all_entries) {
  const auto result = romulus::data::probe_win95_data_container_file(data_root, candidate_arg);
  if (!result.ok()) {
    romulus::core::log_error(result.error.value().message);
    return 1;
  }

  std::cout << romulus::data::format_win95_data_container_report(
      result.value.value(),
      candidate_arg,
      romulus::data::Win95PackReportOptions{
          .preview_entry_limit = 8,
          .include_all_entries = include_all_entries,
      });
  return 0;
}

[[nodiscard]] std::optional<romulus::data::RgbaImage> decode_lbm_to_rgba(const std::filesystem::path& data_root,
                                                                          const std::string& lbm_file_arg) {
  const auto lbm_path = resolve_data_relative(data_root, lbm_file_arg);
  const auto loaded = romulus::data::load_file_to_memory(lbm_path);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error.value().message);
    return std::nullopt;
  }

  const auto parsed = romulus::data::parse_ilbm_image(loaded.value.value().bytes);
  if (!parsed.ok()) {
    romulus::core::log_error(parsed.error.value().message);
    return std::nullopt;
  }

  const auto rgba = romulus::data::convert_ilbm_to_rgba(parsed.value.value());
  if (!rgba.ok()) {
    romulus::core::log_error(rgba.error.value().message);
    return std::nullopt;
  }

  return rgba.value.value();
}

int run_lbm_export(const std::filesystem::path& data_root,
                   const std::string& lbm_file_arg,
                   const std::string& output_file_arg) {
  const auto output_path = resolve_data_relative(data_root, output_file_arg);
  const auto rgba = decode_lbm_to_rgba(data_root, lbm_file_arg);
  if (!rgba.has_value()) {
    return 1;
  }

  const auto exported = romulus::data::export_rgba_image_as_ppm(rgba.value(), output_path);
  if (!exported.ok()) {
    romulus::core::log_error(exported.error->message);
    return 1;
  }

  romulus::core::log_info("Exported decoded LBM to: " + output_path.string());
  return 0;
}

int run_lbm_viewer(const std::filesystem::path& data_root, const std::string& lbm_file_arg, bool smoke_test) {
  const auto rgba = decode_lbm_to_rgba(data_root, lbm_file_arg);
  if (!rgba.has_value()) {
    return 1;
  }

  romulus::platform::Application app({
      .smoke_test = smoke_test,
      .data_root = data_root,
      .debug_view_image = rgba,
  });
  return app.run();
}

[[nodiscard]] std::optional<romulus::data::Win95PackIlbmExtraction> decode_pack_ilbm_entry(
    const std::filesystem::path& data_root,
    const std::string& container_file_arg,
    std::size_t entry_index) {
  const auto container_path = resolve_data_relative(data_root, container_file_arg);
  const auto loaded = romulus::data::load_file_to_memory(container_path);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error.value().message);
    return std::nullopt;
  }

  const auto parsed_container = romulus::data::parse_win95_pack_container(loaded.value.value().bytes);
  if (!parsed_container.ok()) {
    romulus::core::log_error(parsed_container.error.value().message);
    return std::nullopt;
  }

  const auto extracted = romulus::data::extract_win95_pack_ilbm_entry(
      loaded.value.value().bytes,
      parsed_container.value.value(),
      entry_index);
  if (!extracted.ok()) {
    romulus::core::log_error(extracted.error.value().message);
    return std::nullopt;
  }

  return extracted.value.value();
}

int run_pack_ilbm_probe(const std::filesystem::path& data_root,
                        const std::string& container_file_arg,
                        const std::size_t entry_index) {
  const auto extracted = decode_pack_ilbm_entry(data_root, container_file_arg, entry_index);
  if (!extracted.has_value()) {
    return 1;
  }

  std::cout << romulus::data::format_lbm_report(extracted->ilbm);
  return 0;
}

int run_pack_ilbm_batch_probe(const std::filesystem::path& data_root,
                              const std::string& container_file_arg,
                              const bool include_all_entries) {
  const auto container_path = resolve_data_relative(data_root, container_file_arg);
  const auto loaded = romulus::data::load_file_to_memory(container_path);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error.value().message);
    return 1;
  }

  const auto parsed_container = romulus::data::parse_win95_pack_container(loaded.value.value().bytes);
  if (!parsed_container.ok()) {
    romulus::core::log_error(parsed_container.error.value().message);
    return 1;
  }

  const auto batch_result = romulus::data::analyze_win95_pack_ilbm_batch(
      loaded.value.value().bytes,
      parsed_container.value.value());
  std::cout << romulus::data::format_win95_pack_ilbm_batch_report(
      batch_result,
      container_file_arg,
      romulus::data::Win95PackIlbmBatchReportOptions{
          .preview_entry_limit = 8,
          .include_all_entries = include_all_entries,
      });
  return 0;
}

int run_pack_ilbm_export(const std::filesystem::path& data_root,
                         const std::string& container_file_arg,
                         const std::size_t entry_index,
                         const std::string& output_file_arg) {
  const auto extracted = decode_pack_ilbm_entry(data_root, container_file_arg, entry_index);
  if (!extracted.has_value()) {
    return 1;
  }

  const auto rgba = romulus::data::convert_ilbm_to_rgba(extracted->ilbm);
  if (!rgba.ok()) {
    romulus::core::log_error(rgba.error.value().message);
    return 1;
  }

  const auto output_path = resolve_data_relative(data_root, output_file_arg);
  const auto exported = romulus::data::export_rgba_image_as_ppm(rgba.value.value(), output_path);
  if (!exported.ok()) {
    romulus::core::log_error(exported.error.value().message);
    return 1;
  }

  romulus::core::log_info("Exported PACK ILBM entry to: " + output_path.string());
  return 0;
}

int run_pack_ilbm_viewer(const std::filesystem::path& data_root,
                         const std::string& container_file_arg,
                         const std::size_t entry_index,
                         const bool smoke_test) {
  const auto extracted = decode_pack_ilbm_entry(data_root, container_file_arg, entry_index);
  if (!extracted.has_value()) {
    return 1;
  }

  const auto rgba = romulus::data::convert_ilbm_to_rgba(extracted->ilbm);
  if (!rgba.ok()) {
    romulus::core::log_error(rgba.error.value().message);
    return 1;
  }

  romulus::platform::Application app({
      .smoke_test = smoke_test,
      .data_root = data_root,
      .debug_view_image = rgba.value.value(),
  });
  return app.run();
}

}  // namespace

int main(int argc, char* argv[]) {
  const auto parsed = parse_arguments(argc, argv);
  if (!parsed.has_value()) {
    romulus::core::log_error(
        "Usage: caesar2 [--smoke-test] [--data-dir <path>] [--inventory-manifest] [--manifest-out <path>] "
        "[--probe-file <path>] [--probe-candidate <path>] [--match-signature <path>] "
        "[--probe-lbm <path>] [--probe-pl8 <path> ...] "
        "[--probe-exe <path>] "
        "[--probe-exe-resources <path>] "
        "[--probe-exe-resource-payloads <path>] "
        "[--probe-win95-data] "
        "[--probe-win95-container <path>] "
        "[--probe-win95-container-entries-all] "
        "[--probe-pack-ilbm <container> <entry_index>] "
        "[--probe-pack-ilbm-batch <container>] "
        "[--probe-pack-ilbm-batch-entries-all] "
        "[--extract-pack-ilbm <container> <entry_index> --export-output <path>] "
        "[--view-pack-ilbm <container> <entry_index>] "
        "[--export-lbm-file <path> --export-output <path>] "
        "[--view-lbm-file <path>] "
        "[--classify-candidate <path> ...] [--classify-include-secondary] "
        "[--export-tile-file <path> --export-palette-file <path> --export-output <path> "
        "[--index-zero-transparent]] [--view-tile-file <path> --view-palette-file <path> "
        "[--index-zero-transparent]]");
    return 1;
  }

  const auto config_path = romulus::platform::default_startup_config_path();
  std::optional<std::filesystem::path> preferred_data_root;

  if (parsed->data_dir.has_value()) {
    preferred_data_root = romulus::data::resolve_data_root(parsed->data_dir.value());
  } else if (const auto persisted = romulus::platform::load_persisted_data_root(config_path); persisted.has_value()) {
    preferred_data_root = romulus::data::resolve_data_root(persisted->string());
  }

  const auto startup = romulus::platform::evaluate_startup_data_root(preferred_data_root);
  const std::filesystem::path data_root = startup.data_root.value_or(std::filesystem::path("."));


  const bool has_data_required_command = parsed->inventory_manifest || parsed->probe_file.has_value() ||
                                         parsed->probe_lbm_file.has_value() || !parsed->probe_pl8_files.empty() ||
                                         parsed->probe_exe_file.has_value() || parsed->probe_exe_resources_file.has_value() ||
                                         parsed->probe_exe_resource_payloads_file.has_value() ||
                                         parsed->probe_win95_data || parsed->probe_win95_container_file.has_value() ||
                                         parsed->probe_pack_ilbm_container_file.has_value() ||
                                         parsed->probe_pack_ilbm_batch_container_file.has_value() ||
                                         parsed->extract_pack_ilbm_container_file.has_value() ||
                                         parsed->view_pack_ilbm_container_file.has_value() ||
                                         parsed->export_lbm_file.has_value() || parsed->view_lbm_file.has_value() ||
                                         !parsed->probe_candidates.empty() || parsed->match_signature.has_value() ||
                                         !parsed->classify_candidates.empty() || parsed->export_tile_file.has_value() ||
                                         parsed->view_tile_file.has_value();

  if (has_data_required_command && startup.state != romulus::platform::StartupState::DataRootReady) {
    romulus::core::log_error(startup.message);
    return 1;
  }

  if (parsed->inventory_manifest) {
    return run_manifest_generation(data_root, parsed->inventory_manifest_out);
  }

  if (parsed->probe_file.has_value()) {
    return run_binary_probe(data_root, parsed->probe_file.value());
  }

  if (parsed->probe_lbm_file.has_value()) {
    return run_lbm_probe(data_root, parsed->probe_lbm_file.value());
  }

  if (!parsed->probe_pl8_files.empty()) {
    return run_pl8_probe(data_root, parsed->probe_pl8_files);
  }

  if (parsed->probe_exe_file.has_value()) {
    return run_exe_probe(data_root, parsed->probe_exe_file.value());
  }

  if (parsed->probe_exe_resources_file.has_value()) {
    return run_exe_resource_probe(data_root, parsed->probe_exe_resources_file.value());
  }

  if (parsed->probe_exe_resource_payloads_file.has_value()) {
    return run_exe_resource_payload_probe(data_root, parsed->probe_exe_resource_payloads_file.value());
  }

  if (parsed->probe_win95_data) {
    return run_win95_data_probe(data_root);
  }

  if (parsed->probe_win95_container_file.has_value()) {
    return run_win95_container_probe(data_root,
                                     parsed->probe_win95_container_file.value(),
                                     parsed->probe_win95_container_entries_all);
  }

  if (parsed->probe_pack_ilbm_container_file.has_value()) {
    return run_pack_ilbm_probe(data_root,
                               parsed->probe_pack_ilbm_container_file.value(),
                               parsed->probe_pack_ilbm_entry_index.value());
  }

  if (parsed->probe_pack_ilbm_batch_container_file.has_value()) {
    return run_pack_ilbm_batch_probe(data_root,
                                     parsed->probe_pack_ilbm_batch_container_file.value(),
                                     parsed->probe_pack_ilbm_batch_entries_all);
  }

  if (parsed->extract_pack_ilbm_container_file.has_value()) {
    return run_pack_ilbm_export(data_root,
                                parsed->extract_pack_ilbm_container_file.value(),
                                parsed->extract_pack_ilbm_entry_index.value(),
                                parsed->export_output_file.value());
  }

  if (parsed->view_pack_ilbm_container_file.has_value()) {
    return run_pack_ilbm_viewer(data_root,
                                parsed->view_pack_ilbm_container_file.value(),
                                parsed->view_pack_ilbm_entry_index.value(),
                                parsed->smoke_test);
  }

  if (parsed->export_lbm_file.has_value()) {
    return run_lbm_export(data_root, parsed->export_lbm_file.value(), parsed->export_output_file.value());
  }

  if (parsed->view_lbm_file.has_value()) {
    return run_lbm_viewer(data_root, parsed->view_lbm_file.value(), parsed->smoke_test);
  }

  if (!parsed->probe_candidates.empty()) {
    return run_candidate_probe(data_root, parsed->probe_candidates);
  }

  if (parsed->match_signature.has_value()) {
    return run_signature_match(data_root, parsed->match_signature.value());
  }

  if (!parsed->classify_candidates.empty()) {
    return run_batch_classification(data_root, parsed->classify_candidates, parsed->classify_include_secondary);
  }

  if (parsed->export_tile_file.has_value()) {
    return run_tile_export(data_root,
                           parsed->export_tile_file.value(),
                           parsed->export_palette_file.value(),
                           parsed->export_output_file.value(),
                           parsed->index_zero_transparent);
  }

  if (parsed->view_tile_file.has_value()) {
    return run_tile_viewer(data_root,
                           parsed->view_tile_file.value(),
                           parsed->view_palette_file.value(),
                           parsed->smoke_test,
                           parsed->index_zero_transparent);
  }

  romulus::platform::Application app({
      .smoke_test = parsed->smoke_test,
      .data_root = preferred_data_root,
      .startup_config_path = config_path,
  });

  return app.run();
}
