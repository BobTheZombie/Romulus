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
#include "romulus/data/pl8_image_resource.h"
#include "romulus/data/pl8_sprite_table_resource.h"
#include "romulus/data/image256_resource.h"
#include "romulus/data/pe_exe_resource.h"
#include "romulus/data/win95_data_container.h"
#include "romulus/data/win95_data_probe.h"
#include "romulus/platform/application.h"
#include "romulus/platform/startup.h"

namespace {

constexpr std::size_t k_max_pack_ilbm_batch_exports = 64;
constexpr std::size_t k_max_pack_text_batch_exports = 64;

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
  std::optional<std::string> probe_256_file;
  std::optional<std::string> view_256_file;
  std::optional<std::string> view_256_palette_file;
  std::optional<std::string> export_256_file;
  std::optional<std::string> export_256_palette_file;
  std::optional<std::string> probe_pl8_image_file;
  std::optional<std::string> probe_pl8_image_variant_file;
  std::optional<std::string> probe_pl8_structured_file;
  std::optional<std::string> probe_pl8_structured_regions_file;
  std::optional<std::string> compare_pl8_image_variant_lhs_file;
  std::optional<std::string> compare_pl8_image_variant_rhs_file;
  std::optional<std::string> compare_pl8_structured_regions_lhs_file;
  std::optional<std::string> compare_pl8_structured_regions_rhs_file;
  std::optional<std::string> view_pl8_image_file;
  std::optional<std::string> view_pl8_image_palette_file;
  std::optional<std::string> export_pl8_image_file;
  std::optional<std::string> export_pl8_image_palette_file;
  std::optional<std::string> view_pl8_structured_file;
  std::optional<std::string> view_pl8_structured_palette_file;
  std::optional<std::string> export_pl8_structured_file;
  std::optional<std::string> export_pl8_structured_palette_file;
  std::optional<std::string> probe_pl8_sprites_file;
  std::optional<std::string> view_pl8_sprite_file;
  std::optional<std::string> view_pl8_sprite_palette_file;
  std::optional<std::string> export_pl8_sprite_file;
  std::optional<std::string> export_pl8_sprite_palette_file;
  std::optional<std::size_t> pl8_sprite_index;
  std::optional<std::size_t> image_width;
  std::optional<std::size_t> image_height;
  bool probe_win95_data = false;
  std::optional<std::string> probe_win95_container_file;
  bool probe_win95_container_entries_all = false;
  std::optional<std::string> probe_pack_ilbm_container_file;
  std::optional<std::size_t> probe_pack_ilbm_entry_index;
  std::optional<std::string> probe_pack_text_container_file;
  std::optional<std::size_t> probe_pack_text_entry_index;
  std::optional<std::string> probe_pack_pl8_container_file;
  std::optional<std::size_t> probe_pack_pl8_entry_index;
  std::optional<std::string> probe_pack_text_batch_container_file;
  bool probe_pack_text_batch_entries_all = false;
  std::optional<std::string> probe_pack_ilbm_batch_container_file;
  bool probe_pack_ilbm_batch_entries_all = false;
  std::optional<std::string> index_pack_text_container_file;
  bool index_pack_text_entries_all = false;
  std::optional<std::string> index_pack_ilbm_container_file;
  bool index_pack_ilbm_entries_all = false;
  std::optional<std::string> index_pack_known_container_file;
  bool index_pack_known_entries_all = false;
  std::optional<std::string> export_pack_text_success_container_file;
  std::optional<std::size_t> export_pack_text_success_entry_index;
  std::optional<std::string> export_pack_text_first_container_file;
  std::optional<std::size_t> export_pack_text_first_count;
  std::optional<std::string> export_pack_ilbm_success_container_file;
  std::optional<std::size_t> export_pack_ilbm_success_entry_index;
  std::optional<std::string> export_pack_ilbm_first_container_file;
  std::optional<std::size_t> export_pack_ilbm_first_count;
  std::optional<std::string> export_output_dir;
  std::optional<std::string> extract_pack_ilbm_container_file;
  std::optional<std::size_t> extract_pack_ilbm_entry_index;
  std::optional<std::string> view_pack_ilbm_container_file;
  std::optional<std::size_t> view_pack_ilbm_entry_index;
  std::optional<std::string> export_pack_text_container_file;
  std::optional<std::size_t> export_pack_text_entry_index;
  std::optional<std::string> export_pack_pl8_container_file;
  std::optional<std::size_t> export_pack_pl8_entry_index;
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

    if (argument == "--probe-256") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --probe-256.");
        return std::nullopt;
      }

      parsed.probe_256_file = argv[++index];
      continue;
    }

    if (argument == "--view-256-pl8") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--view-256-pl8 requires <image256> <palettepl8>.");
        return std::nullopt;
      }

      parsed.view_256_file = argv[++index];
      parsed.view_256_palette_file = argv[++index];
      continue;
    }

    if (argument == "--export-256-pl8") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--export-256-pl8 requires <image256> <palettepl8>.");
        return std::nullopt;
      }

      parsed.export_256_file = argv[++index];
      parsed.export_256_palette_file = argv[++index];
      continue;
    }

    if (argument == "--probe-pl8-image") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --probe-pl8-image.");
        return std::nullopt;
      }

      parsed.probe_pl8_image_file = argv[++index];
      continue;
    }

    if (argument == "--probe-pl8-sprites") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --probe-pl8-sprites.");
        return std::nullopt;
      }

      parsed.probe_pl8_sprites_file = argv[++index];
      continue;
    }

    if (argument == "--probe-pl8-image-variant") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --probe-pl8-image-variant.");
        return std::nullopt;
      }

      parsed.probe_pl8_image_variant_file = argv[++index];
      continue;
    }

    if (argument == "--probe-pl8-structured") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --probe-pl8-structured.");
        return std::nullopt;
      }

      parsed.probe_pl8_structured_file = argv[++index];
      continue;
    }

    if (argument == "--probe-pl8-structured-regions") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --probe-pl8-structured-regions.");
        return std::nullopt;
      }

      parsed.probe_pl8_structured_regions_file = argv[++index];
      continue;
    }

    if (argument == "--compare-pl8-image-variants") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--compare-pl8-image-variants requires <lhs_imagepl8> <rhs_imagepl8>.");
        return std::nullopt;
      }

      parsed.compare_pl8_image_variant_lhs_file = argv[++index];
      parsed.compare_pl8_image_variant_rhs_file = argv[++index];
      continue;
    }

    if (argument == "--compare-pl8-structured-regions") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--compare-pl8-structured-regions requires <lhs_imagepl8> <rhs_imagepl8>.");
        return std::nullopt;
      }

      parsed.compare_pl8_structured_regions_lhs_file = argv[++index];
      parsed.compare_pl8_structured_regions_rhs_file = argv[++index];
      continue;
    }

    if (argument == "--view-pl8-image-pair") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--view-pl8-image-pair requires <imagepl8> <palette256>.");
        return std::nullopt;
      }

      parsed.view_pl8_image_file = argv[++index];
      parsed.view_pl8_image_palette_file = argv[++index];
      continue;
    }

    if (argument == "--view-pl8-sprite-pair") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--view-pl8-sprite-pair requires <imagepl8> <palette256>.");
        return std::nullopt;
      }

      parsed.view_pl8_sprite_file = argv[++index];
      parsed.view_pl8_sprite_palette_file = argv[++index];
      continue;
    }

    if (argument == "--export-pl8-image-pair") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--export-pl8-image-pair requires <imagepl8> <palette256>.");
        return std::nullopt;
      }

      parsed.export_pl8_image_file = argv[++index];
      parsed.export_pl8_image_palette_file = argv[++index];
      continue;
    }

    if (argument == "--export-pl8-sprite-pair") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--export-pl8-sprite-pair requires <imagepl8> <palette256>.");
        return std::nullopt;
      }

      parsed.export_pl8_sprite_file = argv[++index];
      parsed.export_pl8_sprite_palette_file = argv[++index];
      continue;
    }

    if (argument == "--view-pl8-structured-pair") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--view-pl8-structured-pair requires <imagepl8> <palette256>.");
        return std::nullopt;
      }

      parsed.view_pl8_structured_file = argv[++index];
      parsed.view_pl8_structured_palette_file = argv[++index];
      continue;
    }

    if (argument == "--export-pl8-structured-pair") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--export-pl8-structured-pair requires <imagepl8> <palette256>.");
        return std::nullopt;
      }

      parsed.export_pl8_structured_file = argv[++index];
      parsed.export_pl8_structured_palette_file = argv[++index];
      continue;
    }

    if (argument == "--width") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --width.");
        return std::nullopt;
      }

      const auto parsed_value = parse_size_t_argument(argv[++index]);
      if (!parsed_value.has_value()) {
        romulus::core::log_error("Invalid --width value; expected a non-negative integer.");
        return std::nullopt;
      }
      parsed.image_width = parsed_value.value();
      continue;
    }

    if (argument == "--sprite-index") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --sprite-index.");
        return std::nullopt;
      }

      const auto parsed_index = parse_size_t_argument(argv[++index]);
      if (!parsed_index.has_value()) {
        romulus::core::log_error("--sprite-index requires a non-negative integer value.");
        return std::nullopt;
      }

      parsed.pl8_sprite_index = parsed_index.value();
      continue;
    }

    if (argument == "--height") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --height.");
        return std::nullopt;
      }

      const auto parsed_value = parse_size_t_argument(argv[++index]);
      if (!parsed_value.has_value()) {
        romulus::core::log_error("Invalid --height value; expected a non-negative integer.");
        return std::nullopt;
      }
      parsed.image_height = parsed_value.value();
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

    if (argument == "--probe-pack-text") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--probe-pack-text requires <container> <entry_index>.");
        return std::nullopt;
      }

      parsed.probe_pack_text_container_file = argv[++index];
      const auto parsed_index = parse_size_t_argument(argv[++index]);
      if (!parsed_index.has_value()) {
        romulus::core::log_error("Invalid --probe-pack-text entry_index; expected a non-negative integer.");
        return std::nullopt;
      }

      parsed.probe_pack_text_entry_index = parsed_index;
      continue;
    }

    if (argument == "--probe-pack-pl8") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--probe-pack-pl8 requires <container> <entry_index>.");
        return std::nullopt;
      }

      parsed.probe_pack_pl8_container_file = argv[++index];
      const auto parsed_index = parse_size_t_argument(argv[++index]);
      if (!parsed_index.has_value()) {
        romulus::core::log_error("Invalid --probe-pack-pl8 entry_index; expected a non-negative integer.");
        return std::nullopt;
      }

      parsed.probe_pack_pl8_entry_index = parsed_index;
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

    if (argument == "--probe-pack-text-batch") {
      if (index + 1 >= argc) {
        romulus::core::log_error("--probe-pack-text-batch requires <container>.");
        return std::nullopt;
      }

      parsed.probe_pack_text_batch_container_file = argv[++index];
      continue;
    }

    if (argument == "--probe-pack-text-batch-entries-all") {
      parsed.probe_pack_text_batch_entries_all = true;
      continue;
    }

    if (argument == "--probe-pack-ilbm-batch-entries-all") {
      parsed.probe_pack_ilbm_batch_entries_all = true;
      continue;
    }

    if (argument == "--index-pack-text") {
      if (index + 1 >= argc) {
        romulus::core::log_error("--index-pack-text requires <container>.");
        return std::nullopt;
      }

      parsed.index_pack_text_container_file = argv[++index];
      continue;
    }

    if (argument == "--index-pack-text-entries-all") {
      parsed.index_pack_text_entries_all = true;
      continue;
    }

    if (argument == "--index-pack-ilbm") {
      if (index + 1 >= argc) {
        romulus::core::log_error("--index-pack-ilbm requires <container>.");
        return std::nullopt;
      }

      parsed.index_pack_ilbm_container_file = argv[++index];
      continue;
    }

    if (argument == "--index-pack-ilbm-entries-all") {
      parsed.index_pack_ilbm_entries_all = true;
      continue;
    }

    if (argument == "--index-pack-known") {
      if (index + 1 >= argc) {
        romulus::core::log_error("--index-pack-known requires <container>.");
        return std::nullopt;
      }

      parsed.index_pack_known_container_file = argv[++index];
      continue;
    }

    if (argument == "--index-pack-known-entries-all") {
      parsed.index_pack_known_entries_all = true;
      continue;
    }

    if (argument == "--export-pack-ilbm-success") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--export-pack-ilbm-success requires <container> <entry_index>.");
        return std::nullopt;
      }

      parsed.export_pack_ilbm_success_container_file = argv[++index];
      const auto parsed_index = parse_size_t_argument(argv[++index]);
      if (!parsed_index.has_value()) {
        romulus::core::log_error("Invalid --export-pack-ilbm-success entry_index; expected a non-negative integer.");
        return std::nullopt;
      }

      parsed.export_pack_ilbm_success_entry_index = parsed_index;
      continue;
    }

    if (argument == "--export-pack-text-success") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--export-pack-text-success requires <container> <entry_index>.");
        return std::nullopt;
      }

      parsed.export_pack_text_success_container_file = argv[++index];
      const auto parsed_index = parse_size_t_argument(argv[++index]);
      if (!parsed_index.has_value()) {
        romulus::core::log_error("Invalid --export-pack-text-success entry_index; expected a non-negative integer.");
        return std::nullopt;
      }

      parsed.export_pack_text_success_entry_index = parsed_index;
      continue;
    }

    if (argument == "--export-pack-text-first") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--export-pack-text-first requires <container> <count>.");
        return std::nullopt;
      }

      parsed.export_pack_text_first_container_file = argv[++index];
      const auto parsed_count = parse_size_t_argument(argv[++index]);
      if (!parsed_count.has_value()) {
        romulus::core::log_error("Invalid --export-pack-text-first count; expected a non-negative integer.");
        return std::nullopt;
      }

      parsed.export_pack_text_first_count = parsed_count;
      continue;
    }

    if (argument == "--export-pack-ilbm-first") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--export-pack-ilbm-first requires <container> <count>.");
        return std::nullopt;
      }

      parsed.export_pack_ilbm_first_container_file = argv[++index];
      const auto parsed_count = parse_size_t_argument(argv[++index]);
      if (!parsed_count.has_value()) {
        romulus::core::log_error("Invalid --export-pack-ilbm-first count; expected a non-negative integer.");
        return std::nullopt;
      }

      parsed.export_pack_ilbm_first_count = parsed_count;
      continue;
    }

    if (argument == "--export-output-dir") {
      if (index + 1 >= argc) {
        romulus::core::log_error("Missing value after --export-output-dir.");
        return std::nullopt;
      }

      parsed.export_output_dir = argv[++index];
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

    if (argument == "--export-pack-text") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--export-pack-text requires <container> <entry_index>.");
        return std::nullopt;
      }

      parsed.export_pack_text_container_file = argv[++index];
      const auto parsed_index = parse_size_t_argument(argv[++index]);
      if (!parsed_index.has_value()) {
        romulus::core::log_error("Invalid --export-pack-text entry_index; expected a non-negative integer.");
        return std::nullopt;
      }

      parsed.export_pack_text_entry_index = parsed_index;
      continue;
    }

    if (argument == "--export-pack-pl8") {
      if (index + 2 >= argc) {
        romulus::core::log_error("--export-pack-pl8 requires <container> <entry_index>.");
        return std::nullopt;
      }

      parsed.export_pack_pl8_container_file = argv[++index];
      const auto parsed_index = parse_size_t_argument(argv[++index]);
      if (!parsed_index.has_value()) {
        romulus::core::log_error("Invalid --export-pack-pl8 entry_index; expected a non-negative integer.");
        return std::nullopt;
      }

      parsed.export_pack_pl8_entry_index = parsed_index;
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

  const bool has_lbm_export_arg = parsed.export_lbm_file.has_value();
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

  const bool has_256_probe_arg = parsed.probe_256_file.has_value();
  const bool has_256_view_arg = parsed.view_256_file.has_value() || parsed.view_256_palette_file.has_value();
  const bool has_256_export_arg =
      parsed.export_256_file.has_value() || parsed.export_256_palette_file.has_value() ||
      (parsed.export_output_file.has_value() && parsed.export_256_file.has_value());
  if (has_256_view_arg) {
    if (!parsed.view_256_file.has_value() || !parsed.view_256_palette_file.has_value()) {
      romulus::core::log_error("--view-256-pl8 requires <image256> <palettepl8>.");
      return std::nullopt;
    }
  }
  if (has_256_export_arg) {
    if (!parsed.export_256_file.has_value() || !parsed.export_256_palette_file.has_value() ||
        !parsed.export_output_file.has_value()) {
      romulus::core::log_error("--export-256-pl8 requires <image256> <palettepl8> --export-output <path>.");
      return std::nullopt;
    }
  }

  const bool has_any_256_mode = has_256_probe_arg || has_256_view_arg || has_256_export_arg;
  const bool has_pl8_image_probe_arg = parsed.probe_pl8_image_file.has_value();
  const bool has_pl8_variant_probe_arg = parsed.probe_pl8_image_variant_file.has_value();
  const bool has_pl8_structured_probe_arg = parsed.probe_pl8_structured_file.has_value();
  const bool has_pl8_structured_regions_probe_arg = parsed.probe_pl8_structured_regions_file.has_value();
  const bool has_pl8_variant_compare_arg =
      parsed.compare_pl8_image_variant_lhs_file.has_value() || parsed.compare_pl8_image_variant_rhs_file.has_value();
  const bool has_pl8_structured_regions_compare_arg = parsed.compare_pl8_structured_regions_lhs_file.has_value() ||
                                                      parsed.compare_pl8_structured_regions_rhs_file.has_value();
  const bool has_pl8_image_view_arg =
      parsed.view_pl8_image_file.has_value() || parsed.view_pl8_image_palette_file.has_value();
  const bool has_pl8_image_export_arg =
      parsed.export_pl8_image_file.has_value() || parsed.export_pl8_image_palette_file.has_value() ||
      (parsed.export_output_file.has_value() && parsed.export_pl8_image_file.has_value());
  const bool has_pl8_structured_view_arg =
      parsed.view_pl8_structured_file.has_value() || parsed.view_pl8_structured_palette_file.has_value();
  const bool has_pl8_structured_export_arg =
      parsed.export_pl8_structured_file.has_value() || parsed.export_pl8_structured_palette_file.has_value() ||
      (parsed.export_output_file.has_value() && parsed.export_pl8_structured_file.has_value());
  const bool has_pl8_sprite_probe_arg = parsed.probe_pl8_sprites_file.has_value();
  const bool has_pl8_sprite_view_arg =
      parsed.view_pl8_sprite_file.has_value() || parsed.view_pl8_sprite_palette_file.has_value();
  const bool has_pl8_sprite_export_arg =
      parsed.export_pl8_sprite_file.has_value() || parsed.export_pl8_sprite_palette_file.has_value() ||
      (parsed.export_output_file.has_value() && parsed.export_pl8_sprite_file.has_value());
  const bool has_any_pl8_image_mode =
      has_pl8_image_probe_arg || has_pl8_variant_probe_arg || has_pl8_structured_probe_arg ||
      has_pl8_structured_regions_probe_arg || has_pl8_variant_compare_arg || has_pl8_structured_regions_compare_arg ||
      has_pl8_image_view_arg || has_pl8_image_export_arg || has_pl8_structured_view_arg || has_pl8_structured_export_arg ||
      has_pl8_sprite_probe_arg || has_pl8_sprite_view_arg || has_pl8_sprite_export_arg;
  if (has_pl8_image_view_arg) {
    if (!parsed.view_pl8_image_file.has_value() || !parsed.view_pl8_image_palette_file.has_value()) {
      romulus::core::log_error("--view-pl8-image-pair requires <imagepl8> <palette256>.");
      return std::nullopt;
    }
  }
  if (has_pl8_image_export_arg) {
    if (!parsed.export_pl8_image_file.has_value() || !parsed.export_pl8_image_palette_file.has_value() ||
        !parsed.export_output_file.has_value()) {
      romulus::core::log_error(
          "--export-pl8-image-pair requires <imagepl8> <palette256> --export-output <path>.");
      return std::nullopt;
    }
  }
  if (has_pl8_variant_compare_arg) {
    if (!parsed.compare_pl8_image_variant_lhs_file.has_value() || !parsed.compare_pl8_image_variant_rhs_file.has_value()) {
      romulus::core::log_error("--compare-pl8-image-variants requires <lhs_imagepl8> <rhs_imagepl8>.");
      return std::nullopt;
    }
  }
  if (has_pl8_structured_regions_compare_arg) {
    if (!parsed.compare_pl8_structured_regions_lhs_file.has_value() ||
        !parsed.compare_pl8_structured_regions_rhs_file.has_value()) {
      romulus::core::log_error("--compare-pl8-structured-regions requires <lhs_imagepl8> <rhs_imagepl8>.");
      return std::nullopt;
    }
  }
  if (has_pl8_structured_view_arg) {
    if (!parsed.view_pl8_structured_file.has_value() || !parsed.view_pl8_structured_palette_file.has_value()) {
      romulus::core::log_error("--view-pl8-structured-pair requires <imagepl8> <palette256>.");
      return std::nullopt;
    }
  }
  if (has_pl8_structured_export_arg) {
    if (!parsed.export_pl8_structured_file.has_value() || !parsed.export_pl8_structured_palette_file.has_value() ||
        !parsed.export_output_file.has_value()) {
      romulus::core::log_error(
          "--export-pl8-structured-pair requires <imagepl8> <palette256> --export-output <path>.");
      return std::nullopt;
    }
  }
  if (has_pl8_sprite_view_arg) {
    if (!parsed.view_pl8_sprite_file.has_value() || !parsed.view_pl8_sprite_palette_file.has_value()) {
      romulus::core::log_error("--view-pl8-sprite-pair requires <imagepl8> <palette256>.");
      return std::nullopt;
    }
  }
  if (has_pl8_sprite_export_arg) {
    if (!parsed.export_pl8_sprite_file.has_value() || !parsed.export_pl8_sprite_palette_file.has_value() ||
        !parsed.export_output_file.has_value()) {
      romulus::core::log_error("--export-pl8-sprite-pair requires <imagepl8> <palette256> --export-output <path>.");
      return std::nullopt;
    }
  }
  if (parsed.pl8_sprite_index.has_value() &&
      !(has_pl8_sprite_probe_arg)) {
    romulus::core::log_error("--sprite-index is only supported with --probe-pl8-sprites.");
    return std::nullopt;
  }

  if (has_any_256_mode && (has_any_tile_export_arg || has_any_view_arg || has_lbm_export_arg || has_lbm_view_arg)) {
    romulus::core::log_error(".256+.PL8 commands are mutually exclusive with existing tile/LBM export/view commands.");
    return std::nullopt;
  }
  if (has_any_pl8_image_mode &&
      (has_any_tile_export_arg || has_any_view_arg || has_lbm_export_arg || has_lbm_view_arg || has_any_256_mode)) {
    romulus::core::log_error("PL8-image+.256 commands are mutually exclusive with existing export/view modes.");
    return std::nullopt;
  }

  if (has_any_256_mode) {
    const bool has_other_mode = parsed.inventory_manifest || parsed.probe_file.has_value() ||
                                !parsed.probe_candidates.empty() || parsed.match_signature.has_value() ||
                                !parsed.classify_candidates.empty() || parsed.probe_lbm_file.has_value() ||
                                !parsed.probe_pl8_files.empty() || parsed.probe_exe_file.has_value() ||
                                parsed.probe_exe_resources_file.has_value() ||
                                parsed.probe_exe_resource_payloads_file.has_value() || parsed.probe_win95_data ||
                                parsed.probe_win95_container_file.has_value() ||
                                parsed.probe_pack_ilbm_container_file.has_value() ||
                                parsed.probe_pack_text_container_file.has_value() ||
                                parsed.probe_pack_pl8_container_file.has_value() ||
                                parsed.probe_pack_text_batch_container_file.has_value() ||
                                parsed.probe_pack_ilbm_batch_container_file.has_value() ||
                                parsed.index_pack_text_container_file.has_value() ||
                                parsed.index_pack_ilbm_container_file.has_value() ||
                                parsed.index_pack_known_container_file.has_value() ||
                                parsed.export_pack_text_success_container_file.has_value() ||
                                parsed.export_pack_text_first_container_file.has_value() ||
                                parsed.export_pack_ilbm_success_container_file.has_value() ||
                                parsed.export_pack_ilbm_first_container_file.has_value() ||
                                parsed.extract_pack_ilbm_container_file.has_value() ||
                                parsed.view_pack_ilbm_container_file.has_value() ||
                                parsed.export_pack_text_container_file.has_value() ||
                                parsed.export_pack_pl8_container_file.has_value();
    if (has_other_mode) {
      romulus::core::log_error(".256+.PL8 commands are mutually exclusive with unrelated command modes.");
      return std::nullopt;
    }
  }

  if (has_any_pl8_image_mode) {
    const bool has_other_mode = parsed.inventory_manifest || parsed.probe_file.has_value() ||
                                !parsed.probe_candidates.empty() || parsed.match_signature.has_value() ||
                                !parsed.classify_candidates.empty() || parsed.probe_lbm_file.has_value() ||
                                !parsed.probe_pl8_files.empty() || parsed.probe_exe_file.has_value() ||
                                parsed.probe_exe_resources_file.has_value() ||
                                parsed.probe_exe_resource_payloads_file.has_value() || parsed.probe_win95_data ||
                                parsed.probe_win95_container_file.has_value() ||
                                parsed.probe_pack_ilbm_container_file.has_value() ||
                                parsed.probe_pack_text_container_file.has_value() ||
                                parsed.probe_pack_pl8_container_file.has_value() ||
                                parsed.probe_pack_text_batch_container_file.has_value() ||
                                parsed.probe_pack_ilbm_batch_container_file.has_value() ||
                                parsed.index_pack_text_container_file.has_value() ||
                                parsed.index_pack_ilbm_container_file.has_value() ||
                                parsed.index_pack_known_container_file.has_value() ||
                                parsed.export_pack_text_success_container_file.has_value() ||
                                parsed.export_pack_text_first_container_file.has_value() ||
                                parsed.export_pack_ilbm_success_container_file.has_value() ||
                                parsed.export_pack_ilbm_first_container_file.has_value() ||
                                parsed.extract_pack_ilbm_container_file.has_value() ||
                                parsed.view_pack_ilbm_container_file.has_value() ||
                                parsed.export_pack_text_container_file.has_value() ||
                                parsed.export_pack_pl8_container_file.has_value();
    if (has_other_mode) {
      romulus::core::log_error("PL8-image+.256 commands are mutually exclusive with unrelated command modes.");
      return std::nullopt;
    }
  }

  if ((parsed.image_width.has_value() || parsed.image_height.has_value()) && !has_any_256_mode) {
    romulus::core::log_error("--width/--height are only supported with --probe-256, --view-256-pl8, or --export-256-pl8.");
    return std::nullopt;
  }

  if (has_any_256_mode) {
    if (parsed.image_width.has_value() != parsed.image_height.has_value()) {
      romulus::core::log_error("--width and --height must be provided together.");
      return std::nullopt;
    }

    if (parsed.image_width.has_value() && (parsed.image_width.value() == 0 || parsed.image_width.value() > 1024)) {
      romulus::core::log_error("--width must be between 1 and 1024.");
      return std::nullopt;
    }

    if (parsed.image_height.has_value() && (parsed.image_height.value() == 0 || parsed.image_height.value() > 1024)) {
      romulus::core::log_error("--height must be between 1 and 1024.");
      return std::nullopt;
    }
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
  const bool has_pack_text_probe = parsed.probe_pack_text_container_file.has_value();
  const bool has_pack_pl8_probe = parsed.probe_pack_pl8_container_file.has_value();
  const bool has_pack_text_batch_probe = parsed.probe_pack_text_batch_container_file.has_value();
  const bool has_pack_ilbm_batch_probe = parsed.probe_pack_ilbm_batch_container_file.has_value();
  const bool has_pack_text_index = parsed.index_pack_text_container_file.has_value();
  const bool has_pack_ilbm_index = parsed.index_pack_ilbm_container_file.has_value();
  const bool has_pack_known_index = parsed.index_pack_known_container_file.has_value();
  const bool has_pack_text_success_export = parsed.export_pack_text_success_container_file.has_value();
  const bool has_pack_text_first_export = parsed.export_pack_text_first_container_file.has_value();
  const bool has_pack_ilbm_success_export = parsed.export_pack_ilbm_success_container_file.has_value();
  const bool has_pack_ilbm_first_export = parsed.export_pack_ilbm_first_container_file.has_value();
  const bool has_pack_ilbm_extract = parsed.extract_pack_ilbm_container_file.has_value();
  const bool has_pack_ilbm_view = parsed.view_pack_ilbm_container_file.has_value();
  const bool has_pack_text_export = parsed.export_pack_text_container_file.has_value();
  const bool has_pack_pl8_export = parsed.export_pack_pl8_container_file.has_value();
  const int pack_ilbm_mode_count = static_cast<int>(has_pack_ilbm_probe) +
                                   static_cast<int>(has_pack_text_probe) +
                                   static_cast<int>(has_pack_pl8_probe) +
                                   static_cast<int>(has_pack_text_batch_probe) +
                                   static_cast<int>(has_pack_ilbm_batch_probe) +
                                   static_cast<int>(has_pack_text_index) +
                                   static_cast<int>(has_pack_ilbm_index) +
                                   static_cast<int>(has_pack_known_index) +
                                   static_cast<int>(has_pack_text_success_export) +
                                   static_cast<int>(has_pack_text_first_export) +
                                   static_cast<int>(has_pack_ilbm_success_export) +
                                   static_cast<int>(has_pack_ilbm_first_export) +
                                   static_cast<int>(has_pack_ilbm_extract) +
                                   static_cast<int>(has_pack_ilbm_view) +
                                   static_cast<int>(has_pack_text_export) +
                                   static_cast<int>(has_pack_pl8_export);
  if (pack_ilbm_mode_count > 1) {
    romulus::core::log_error(
        "PACK extraction command modes are mutually exclusive.");
    return std::nullopt;
  }

  if ((has_pack_ilbm_probe && !parsed.probe_pack_ilbm_entry_index.has_value()) ||
      (has_pack_text_probe && !parsed.probe_pack_text_entry_index.has_value()) ||
      (has_pack_pl8_probe && !parsed.probe_pack_pl8_entry_index.has_value()) ||
      (has_pack_text_success_export && !parsed.export_pack_text_success_entry_index.has_value()) ||
      (has_pack_ilbm_success_export && !parsed.export_pack_ilbm_success_entry_index.has_value()) ||
      (has_pack_ilbm_extract && !parsed.extract_pack_ilbm_entry_index.has_value()) ||
      (has_pack_text_export && !parsed.export_pack_text_entry_index.has_value()) ||
      (has_pack_pl8_export && !parsed.export_pack_pl8_entry_index.has_value()) ||
      (has_pack_ilbm_view && !parsed.view_pack_ilbm_entry_index.has_value())) {
    romulus::core::log_error("PACK entry extraction commands require a valid entry index.");
    return std::nullopt;
  }

  if (parsed.probe_pack_ilbm_batch_entries_all && !has_pack_ilbm_batch_probe) {
    romulus::core::log_error("--probe-pack-ilbm-batch-entries-all requires --probe-pack-ilbm-batch.");
    return std::nullopt;
  }
  if (parsed.probe_pack_text_batch_entries_all && !has_pack_text_batch_probe) {
    romulus::core::log_error("--probe-pack-text-batch-entries-all requires --probe-pack-text-batch.");
    return std::nullopt;
  }
  if (parsed.index_pack_text_entries_all && !has_pack_text_index) {
    romulus::core::log_error("--index-pack-text-entries-all requires --index-pack-text.");
    return std::nullopt;
  }
  if (parsed.index_pack_known_entries_all && !has_pack_known_index) {
    romulus::core::log_error("--index-pack-known-entries-all requires --index-pack-known.");
    return std::nullopt;
  }

  if (has_pack_ilbm_probe || has_pack_text_probe || has_pack_pl8_probe || has_pack_text_batch_probe || has_pack_ilbm_batch_probe ||
      has_pack_text_index || has_pack_ilbm_index || has_pack_known_index ||
      has_pack_text_success_export || has_pack_text_first_export ||
      has_pack_ilbm_success_export || has_pack_ilbm_first_export || has_pack_ilbm_extract || has_pack_ilbm_view ||
      has_pack_text_export || has_pack_pl8_export) {
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
      romulus::core::log_error("PACK extraction commands are mutually exclusive with other command modes.");
      return std::nullopt;
    }
  }

  if (has_pack_ilbm_extract && !parsed.export_output_file.has_value()) {
    romulus::core::log_error("--extract-pack-ilbm requires --export-output.");
    return std::nullopt;
  }

  if (has_pack_ilbm_success_export && !parsed.export_output_file.has_value()) {
    romulus::core::log_error("--export-pack-ilbm-success requires --export-output.");
    return std::nullopt;
  }
  if (has_pack_text_success_export && !parsed.export_output_file.has_value()) {
    romulus::core::log_error("--export-pack-text-success requires --export-output.");
    return std::nullopt;
  }

  if (has_pack_ilbm_first_export && !parsed.export_output_dir.has_value()) {
    romulus::core::log_error("--export-pack-ilbm-first requires --export-output-dir.");
    return std::nullopt;
  }
  if (has_pack_text_first_export && !parsed.export_output_dir.has_value()) {
    romulus::core::log_error("--export-pack-text-first requires --export-output-dir.");
    return std::nullopt;
  }

  if (!has_pack_ilbm_first_export && !has_pack_text_first_export && parsed.export_output_dir.has_value()) {
    romulus::core::log_error("--export-output-dir requires --export-pack-ilbm-first or --export-pack-text-first.");
    return std::nullopt;
  }

  if (parsed.index_pack_ilbm_entries_all && !has_pack_ilbm_index) {
    romulus::core::log_error("--index-pack-ilbm-entries-all requires --index-pack-ilbm.");
    return std::nullopt;
  }

  if (has_pack_ilbm_first_export && parsed.export_pack_ilbm_first_count.value_or(0) == 0) {
    romulus::core::log_error("--export-pack-ilbm-first count must be greater than zero.");
    return std::nullopt;
  }

  if (has_pack_ilbm_first_export &&
      parsed.export_pack_ilbm_first_count.value_or(0) > k_max_pack_ilbm_batch_exports) {
    romulus::core::log_error("--export-pack-ilbm-first count exceeds bounded maximum of 64.");
    return std::nullopt;
  }
  if (has_pack_text_first_export && parsed.export_pack_text_first_count.value_or(0) == 0) {
    romulus::core::log_error("--export-pack-text-first count must be greater than zero.");
    return std::nullopt;
  }
  if (has_pack_text_first_export &&
      parsed.export_pack_text_first_count.value_or(0) > k_max_pack_text_batch_exports) {
    romulus::core::log_error("--export-pack-text-first count exceeds bounded maximum of 64.");
    return std::nullopt;
  }

  if (!has_pack_ilbm_extract && !has_pack_ilbm_success_export && !has_pack_text_success_export &&
      !has_pack_text_export && !has_pack_pl8_export && parsed.export_output_file.has_value() &&
      !has_any_tile_export_arg && !has_lbm_export_arg && !has_256_export_arg && !has_pl8_image_export_arg &&
      !has_pl8_sprite_export_arg) {
    romulus::core::log_error("--export-output requires an export mode.");
    return std::nullopt;
  }

  if (has_pack_text_export && !parsed.export_output_file.has_value()) {
    romulus::core::log_error("--export-pack-text requires --export-output.");
    return std::nullopt;
  }

  if (has_pack_pl8_export && !parsed.export_output_file.has_value()) {
    romulus::core::log_error("--export-pack-pl8 requires --export-output.");
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

[[nodiscard]] std::optional<std::pair<std::uint16_t, std::uint16_t>> resolve_256_dimensions(
    const std::string& image_path_arg,
    const std::optional<std::size_t>& width_arg,
    const std::optional<std::size_t>& height_arg) {
  if (width_arg.has_value() && height_arg.has_value()) {
    return std::pair<std::uint16_t, std::uint16_t>{static_cast<std::uint16_t>(width_arg.value()),
                                                    static_cast<std::uint16_t>(height_arg.value())};
  }

  return romulus::data::resolve_known_win95_256_dimensions(std::filesystem::path(image_path_arg));
}

[[nodiscard]] std::optional<romulus::data::Image256Pl8DecodeResult> decode_256_pl8_to_rgba(
    const std::filesystem::path& data_root,
    const std::string& image256_file_arg,
    const std::string& palette_file_arg,
    const std::optional<std::size_t>& width_arg,
    const std::optional<std::size_t>& height_arg,
    const bool index_zero_transparent) {
  const auto image_path = resolve_data_relative(data_root, image256_file_arg);
  const auto palette_path = resolve_data_relative(data_root, palette_file_arg);
  const auto dimensions = resolve_256_dimensions(image256_file_arg, width_arg, height_arg);
  if (!dimensions.has_value()) {
    romulus::core::log_error("No dimensions provided and no known bounded dimensions for .256 target: " + image256_file_arg);
    return std::nullopt;
  }

  const auto loaded_image = romulus::data::load_file_to_memory(image_path);
  if (!loaded_image.ok()) {
    romulus::core::log_error(loaded_image.error->message);
    return std::nullopt;
  }

  const auto loaded_palette = romulus::data::load_file_to_memory(palette_path);
  if (!loaded_palette.ok()) {
    romulus::core::log_error(loaded_palette.error->message);
    return std::nullopt;
  }

  const auto decoded = romulus::data::decode_caesar2_win95_256_pl8_pair(loaded_image.value->bytes,
                                                                         loaded_palette.value->bytes,
                                                                         dimensions->first,
                                                                         dimensions->second,
                                                                         index_zero_transparent);

  romulus::data::Image256Pl8Report report{
      .image256_path = image_path,
      .palette_pl8_path = palette_path,
      .width = dimensions->first,
      .height = dimensions->second,
      .payload_size = loaded_image.value->bytes.size(),
      .palette_entries = decoded.ok() ? decoded.value->palette_pl8.palette_entries.size() : 0,
      .success = decoded.ok(),
      .status = decoded.ok() ? "decode_succeeded" : decoded.error->message,
  };
  std::cout << romulus::data::format_image256_pl8_report(report);

  if (!decoded.ok()) {
    return std::nullopt;
  }

  return decoded.value.value();
}

[[nodiscard]] std::optional<romulus::data::Pl8Image256PairDecodeResult> decode_pl8_image_256_to_rgba(
    const std::filesystem::path& data_root,
    const std::string& image_pl8_file_arg,
    const std::string& palette_256_file_arg,
    const bool index_zero_transparent) {
  const auto image_path = resolve_data_relative(data_root, image_pl8_file_arg);
  const auto palette_path = resolve_data_relative(data_root, palette_256_file_arg);

  const auto loaded_image = romulus::data::load_file_to_memory(image_path);
  if (!loaded_image.ok()) {
    romulus::core::log_error(loaded_image.error->message);
    return std::nullopt;
  }

  const auto loaded_palette = romulus::data::load_file_to_memory(palette_path);
  if (!loaded_palette.ok()) {
    romulus::core::log_error(loaded_palette.error->message);
    return std::nullopt;
  }

  const auto decoded = romulus::data::decode_caesar2_forum_pl8_image_pair(loaded_image.value->bytes,
                                                                           loaded_palette.value->bytes,
                                                                           index_zero_transparent);
  if (!decoded.ok()) {
    romulus::core::log_error(decoded.error->message);
    return std::nullopt;
  }

  return decoded.value.value();
}

[[nodiscard]] std::optional<romulus::data::StructuredPl8Image256PairDecodeResult> decode_pl8_structured_256_to_rgba(
    const std::filesystem::path& data_root,
    const std::string& image_pl8_file_arg,
    const std::string& palette_256_file_arg,
    const bool index_zero_transparent) {
  const auto image_path = resolve_data_relative(data_root, image_pl8_file_arg);
  const auto palette_path = resolve_data_relative(data_root, palette_256_file_arg);

  const auto loaded_image = romulus::data::load_file_to_memory(image_path);
  if (!loaded_image.ok()) {
    romulus::core::log_error(loaded_image.error->message);
    return std::nullopt;
  }

  const auto loaded_palette = romulus::data::load_file_to_memory(palette_path);
  if (!loaded_palette.ok()) {
    romulus::core::log_error(loaded_palette.error->message);
    return std::nullopt;
  }

  const auto decoded = romulus::data::decode_caesar2_rat_back_structured_pl8_image_pair(loaded_image.value->bytes,
                                                                                          loaded_palette.value->bytes,
                                                                                          index_zero_transparent);
  if (!decoded.ok()) {
    romulus::core::log_error(decoded.error->message);
    return std::nullopt;
  }

  return decoded.value.value();
}

[[nodiscard]] std::optional<romulus::data::Pl8SpritePairMultiDecodeResult> decode_pl8_sprite_256_to_rgba(
    const std::filesystem::path& data_root,
    const std::string& image_pl8_file_arg,
    const std::string& palette_256_file_arg,
    const bool index_zero_transparent) {
  const auto image_path = resolve_data_relative(data_root, image_pl8_file_arg);
  const auto palette_path = resolve_data_relative(data_root, palette_256_file_arg);

  const auto loaded_image = romulus::data::load_file_to_memory(image_path);
  if (!loaded_image.ok()) {
    romulus::core::log_error(loaded_image.error->message);
    return std::nullopt;
  }

  const auto loaded_palette = romulus::data::load_file_to_memory(palette_path);
  if (!loaded_palette.ok()) {
    romulus::core::log_error(loaded_palette.error->message);
    return std::nullopt;
  }

  const auto decoded = romulus::data::decode_caesar2_pl8_sprite_pair_multi(loaded_image.value->bytes,
                                                                            loaded_palette.value->bytes,
                                                                            index_zero_transparent);
  if (!decoded.ok()) {
    romulus::core::log_error(decoded.error->message);
    return std::nullopt;
  }

  std::cout << romulus::data::format_pl8_sprite_pair_multi_report(decoded.value.value());

  if (!decoded.value->composition.has_value()) {
    romulus::core::log_error("PL8 sprite-table+.256 decode produced no composited canvas.");
    return std::nullopt;
  }

  return decoded.value.value();
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

int run_256_probe(const std::filesystem::path& data_root,
                  const std::string& image256_file_arg,
                  const std::optional<std::size_t>& width_arg,
                  const std::optional<std::size_t>& height_arg) {
  const auto image_path = resolve_data_relative(data_root, image256_file_arg);
  const auto dimensions = resolve_256_dimensions(image256_file_arg, width_arg, height_arg);
  if (!dimensions.has_value()) {
    romulus::core::log_error("No dimensions provided and no known bounded dimensions for .256 target: " + image256_file_arg);
    return 1;
  }

  const auto loaded = romulus::data::load_file_to_memory(image_path);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error.value().message);
    return 1;
  }

  const auto parsed = romulus::data::parse_caesar2_win95_raw_256(loaded.value->bytes, dimensions->first, dimensions->second);
  if (!parsed.ok()) {
    romulus::data::Image256Pl8Report report{
        .image256_path = image_path,
        .palette_pl8_path = "",
        .width = dimensions->first,
        .height = dimensions->second,
        .payload_size = loaded.value->bytes.size(),
        .palette_entries = 0,
        .success = false,
        .status = parsed.error->message,
    };
    std::cout << romulus::data::format_image256_pl8_report(report);
    return 1;
  }

  std::cout << romulus::data::format_image256_report(parsed.value.value());
  return 0;
}

int run_256_pl8_viewer(const std::filesystem::path& data_root,
                       const std::string& image256_file_arg,
                       const std::string& palette_file_arg,
                       const std::optional<std::size_t>& width_arg,
                       const std::optional<std::size_t>& height_arg,
                       const bool smoke_test,
                       const bool index_zero_transparent) {
  const auto decoded = decode_256_pl8_to_rgba(data_root,
                                              image256_file_arg,
                                              palette_file_arg,
                                              width_arg,
                                              height_arg,
                                              index_zero_transparent);
  if (!decoded.has_value()) {
    return 1;
  }

  romulus::platform::Application app({
      .smoke_test = smoke_test,
      .data_root = data_root,
      .debug_view_image = decoded->rgba_image,
  });
  return app.run();
}

int run_256_pl8_export(const std::filesystem::path& data_root,
                       const std::string& image256_file_arg,
                       const std::string& palette_file_arg,
                       const std::optional<std::size_t>& width_arg,
                       const std::optional<std::size_t>& height_arg,
                       const std::string& output_file_arg,
                       const bool index_zero_transparent) {
  const auto decoded = decode_256_pl8_to_rgba(data_root,
                                              image256_file_arg,
                                              palette_file_arg,
                                              width_arg,
                                              height_arg,
                                              index_zero_transparent);
  if (!decoded.has_value()) {
    return 1;
  }

  const auto output_path = resolve_data_relative(data_root, output_file_arg);
  const auto exported = romulus::data::export_rgba_image_as_ppm(decoded->rgba_image, output_path);
  if (!exported.ok()) {
    romulus::core::log_error(exported.error->message);
    return 1;
  }

  romulus::core::log_info("Exported decoded .256+.PL8 image to: " + output_path.string());
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

int run_pl8_image_probe(const std::filesystem::path& data_root, const std::string& image_pl8_file_arg) {
  const auto image_path = resolve_data_relative(data_root, image_pl8_file_arg);
  const auto loaded = romulus::data::load_file_to_memory(image_path);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error->message);
    return 1;
  }

  const auto parsed = romulus::data::parse_caesar2_forum_pl8_image(loaded.value->bytes);
  if (!parsed.ok()) {
    romulus::core::log_error(parsed.error->message);
    return 1;
  }

  std::cout << romulus::data::format_pl8_image_report(parsed.value.value());
  return 0;
}

int run_pl8_sprite_probe(const std::filesystem::path& data_root, const std::string& image_pl8_file_arg) {
  const auto image_path = resolve_data_relative(data_root, image_pl8_file_arg);
  const auto loaded = romulus::data::load_file_to_memory(image_path);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error->message);
    return 1;
  }

  const auto parsed = romulus::data::parse_caesar2_pl8_sprite_table(loaded.value->bytes);
  if (!parsed.ok()) {
    romulus::core::log_error(parsed.error->message);
    return 1;
  }

  std::cout << romulus::data::format_pl8_sprite_table_report(parsed.value.value());
  return 0;
}

int run_pl8_image_variant_probe(const std::filesystem::path& data_root, const std::string& image_pl8_file_arg) {
  const auto image_path = resolve_data_relative(data_root, image_pl8_file_arg);
  const auto loaded = romulus::data::load_file_to_memory(image_path);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error->message);
    return 1;
  }

  const auto probed = romulus::data::probe_caesar2_large_pl8_image_variant(loaded.value->bytes);
  if (!probed.ok()) {
    romulus::core::log_error(probed.error->message);
    return 1;
  }

  std::cout << "source_path: " << image_path.string() << "\n";
  std::cout << romulus::data::format_pl8_image_variant_probe_report(probed.value.value());
  return 0;
}

int run_pl8_structured_probe(const std::filesystem::path& data_root, const std::string& image_pl8_file_arg) {
  const auto image_path = resolve_data_relative(data_root, image_pl8_file_arg);
  const auto loaded = romulus::data::load_file_to_memory(image_path);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error->message);
    return 1;
  }

  const auto parsed = romulus::data::parse_caesar2_rat_back_structured_pl8_image(loaded.value->bytes);
  if (!parsed.ok()) {
    romulus::core::log_error(parsed.error->message);
    return 1;
  }

  std::cout << "source_path: " << image_path.string() << "\n";
  std::cout << romulus::data::format_pl8_structured_report(parsed.value.value());
  return 0;
}

int run_pl8_structured_regions_probe(const std::filesystem::path& data_root, const std::string& image_pl8_file_arg) {
  const auto image_path = resolve_data_relative(data_root, image_pl8_file_arg);
  const auto loaded = romulus::data::load_file_to_memory(image_path);
  if (!loaded.ok()) {
    romulus::core::log_error(loaded.error->message);
    return 1;
  }

  const auto probed = romulus::data::probe_caesar2_rat_back_structured_pl8_regions(loaded.value->bytes);
  if (!probed.ok()) {
    romulus::core::log_error(probed.error->message);
    return 1;
  }

  std::cout << "source_path: " << image_path.string() << "\n";
  std::cout << romulus::data::format_pl8_structured_regions_probe_report(probed.value.value());
  return 0;
}

int run_pl8_image_variant_compare(const std::filesystem::path& data_root,
                                  const std::string& lhs_pl8_file_arg,
                                  const std::string& rhs_pl8_file_arg) {
  const auto lhs_path = resolve_data_relative(data_root, lhs_pl8_file_arg);
  const auto rhs_path = resolve_data_relative(data_root, rhs_pl8_file_arg);

  const auto lhs_loaded = romulus::data::load_file_to_memory(lhs_path);
  if (!lhs_loaded.ok()) {
    romulus::core::log_error(lhs_loaded.error->message);
    return 1;
  }

  const auto rhs_loaded = romulus::data::load_file_to_memory(rhs_path);
  if (!rhs_loaded.ok()) {
    romulus::core::log_error(rhs_loaded.error->message);
    return 1;
  }

  const auto lhs_probed = romulus::data::probe_caesar2_large_pl8_image_variant(lhs_loaded.value->bytes);
  if (!lhs_probed.ok()) {
    romulus::core::log_error(lhs_probed.error->message);
    return 1;
  }
  const auto rhs_probed = romulus::data::probe_caesar2_large_pl8_image_variant(rhs_loaded.value->bytes);
  if (!rhs_probed.ok()) {
    romulus::core::log_error(rhs_probed.error->message);
    return 1;
  }

  std::cout << romulus::data::format_pl8_image_variant_comparison_report(
      lhs_probed.value.value(), lhs_path.string(), rhs_probed.value.value(), rhs_path.string());
  return 0;
}

int run_pl8_structured_regions_compare(const std::filesystem::path& data_root,
                                       const std::string& lhs_pl8_file_arg,
                                       const std::string& rhs_pl8_file_arg) {
  const auto lhs_path = resolve_data_relative(data_root, lhs_pl8_file_arg);
  const auto rhs_path = resolve_data_relative(data_root, rhs_pl8_file_arg);

  const auto lhs_loaded = romulus::data::load_file_to_memory(lhs_path);
  if (!lhs_loaded.ok()) {
    romulus::core::log_error(lhs_loaded.error->message);
    return 1;
  }

  const auto rhs_loaded = romulus::data::load_file_to_memory(rhs_path);
  if (!rhs_loaded.ok()) {
    romulus::core::log_error(rhs_loaded.error->message);
    return 1;
  }

  const auto lhs_probed = romulus::data::probe_caesar2_rat_back_structured_pl8_regions(lhs_loaded.value->bytes);
  if (!lhs_probed.ok()) {
    romulus::core::log_error(lhs_probed.error->message);
    return 1;
  }
  const auto rhs_probed = romulus::data::probe_caesar2_rat_back_structured_pl8_regions(rhs_loaded.value->bytes);
  if (!rhs_probed.ok()) {
    romulus::core::log_error(rhs_probed.error->message);
    return 1;
  }

  std::cout << romulus::data::format_pl8_structured_regions_comparison_report(
      lhs_probed.value.value(), lhs_path.string(), rhs_probed.value.value(), rhs_path.string());
  return 0;
}

int run_pl8_image_pair_viewer(const std::filesystem::path& data_root,
                              const std::string& image_pl8_file_arg,
                              const std::string& palette_256_file_arg,
                              const bool smoke_test,
                              const bool index_zero_transparent) {
  const auto decoded = decode_pl8_image_256_to_rgba(data_root,
                                                     image_pl8_file_arg,
                                                     palette_256_file_arg,
                                                     index_zero_transparent);
  if (!decoded.has_value()) {
    return 1;
  }

  romulus::platform::Application app({
      .smoke_test = smoke_test,
      .data_root = data_root,
      .debug_view_image = decoded->rgba_image,
  });
  return app.run();
}

int run_pl8_image_pair_export(const std::filesystem::path& data_root,
                              const std::string& image_pl8_file_arg,
                              const std::string& palette_256_file_arg,
                              const std::string& output_file_arg,
                              const bool index_zero_transparent) {
  const auto decoded = decode_pl8_image_256_to_rgba(data_root,
                                                     image_pl8_file_arg,
                                                     palette_256_file_arg,
                                                     index_zero_transparent);
  if (!decoded.has_value()) {
    return 1;
  }

  const auto output_path = resolve_data_relative(data_root, output_file_arg);
  const auto exported = romulus::data::export_rgba_image_as_ppm(decoded->rgba_image, output_path);
  if (!exported.ok()) {
    romulus::core::log_error(exported.error->message);
    return 1;
  }

  romulus::core::log_info("Exported decoded PL8-image+.256 pair to: " + output_path.string());
  return 0;
}

int run_pl8_structured_pair_viewer(const std::filesystem::path& data_root,
                                   const std::string& image_pl8_file_arg,
                                   const std::string& palette_256_file_arg,
                                   const bool smoke_test,
                                   const bool index_zero_transparent) {
  const auto decoded = decode_pl8_structured_256_to_rgba(data_root,
                                                          image_pl8_file_arg,
                                                          palette_256_file_arg,
                                                          index_zero_transparent);
  if (!decoded.has_value()) {
    return 1;
  }

  romulus::platform::Application app({
      .smoke_test = smoke_test,
      .data_root = data_root,
      .debug_view_image = decoded->rgba_image,
  });
  return app.run();
}

int run_pl8_structured_pair_export(const std::filesystem::path& data_root,
                                   const std::string& image_pl8_file_arg,
                                   const std::string& palette_256_file_arg,
                                   const std::string& output_file_arg,
                                   const bool index_zero_transparent) {
  const auto decoded = decode_pl8_structured_256_to_rgba(data_root,
                                                          image_pl8_file_arg,
                                                          palette_256_file_arg,
                                                          index_zero_transparent);
  if (!decoded.has_value()) {
    return 1;
  }

  const auto output_path = resolve_data_relative(data_root, output_file_arg);
  const auto exported = romulus::data::export_rgba_image_as_ppm(decoded->rgba_image, output_path);
  if (!exported.ok()) {
    romulus::core::log_error(exported.error->message);
    return 1;
  }

  romulus::core::log_info("Exported decoded structured-PL8+.256 pair to: " + output_path.string());
  return 0;
}

int run_pl8_sprite_pair_viewer(const std::filesystem::path& data_root,
                               const std::string& image_pl8_file_arg,
                               const std::string& palette_256_file_arg,
                               const bool smoke_test,
                               const bool index_zero_transparent) {
  const auto decoded = decode_pl8_sprite_256_to_rgba(data_root,
                                                      image_pl8_file_arg,
                                                      palette_256_file_arg,
                                                      index_zero_transparent);
  if (!decoded.has_value()) {
    return 1;
  }

  romulus::platform::Application app({
      .smoke_test = smoke_test,
      .data_root = data_root,
      .debug_view_image = decoded->composition->rgba_image,
  });
  return app.run();
}

int run_pl8_sprite_pair_export(const std::filesystem::path& data_root,
                               const std::string& image_pl8_file_arg,
                               const std::string& palette_256_file_arg,
                               const std::filesystem::path& output_path,
                               const bool index_zero_transparent) {
  const auto decoded = decode_pl8_sprite_256_to_rgba(data_root,
                                                      image_pl8_file_arg,
                                                      palette_256_file_arg,
                                                      index_zero_transparent);
  if (!decoded.has_value()) {
    return 1;
  }

  const auto exported = romulus::data::export_rgba_image_as_ppm(decoded->composition->rgba_image, output_path);
  if (!exported.ok()) {
    romulus::core::log_error(exported.error->message);
    return 1;
  }

  romulus::core::log_info("Exported decoded PL8 sprite-table+.256 pair to: " + output_path.string());
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

[[nodiscard]] std::optional<romulus::data::Win95PackTextExtraction> decode_pack_text_entry(
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

  const auto extracted = romulus::data::extract_win95_pack_text_entry(
      loaded.value.value().bytes,
      parsed_container.value.value(),
      entry_index);
  if (!extracted.ok()) {
    romulus::core::log_error(extracted.error.value().message);
    return std::nullopt;
  }

  return extracted.value.value();
}

[[nodiscard]] std::optional<romulus::data::Win95PackPl8Extraction> decode_pack_pl8_entry(
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

  const auto extracted = romulus::data::extract_win95_pack_pl8_entry(
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

int run_pack_text_probe(const std::filesystem::path& data_root,
                        const std::string& container_file_arg,
                        const std::size_t entry_index) {
  const auto extracted = decode_pack_text_entry(data_root, container_file_arg, entry_index);
  if (!extracted.has_value()) {
    return 1;
  }

  std::cout << romulus::data::format_win95_pack_text_report(
      extracted.value(),
      container_file_arg,
      romulus::data::Win95PackTextReportOptions{
          .preview_character_limit = 160,
      });
  return 0;
}

int run_pack_pl8_probe(const std::filesystem::path& data_root,
                       const std::string& container_file_arg,
                       const std::size_t entry_index) {
  const auto extracted = decode_pack_pl8_entry(data_root, container_file_arg, entry_index);
  if (!extracted.has_value()) {
    return 1;
  }

  std::cout << romulus::data::format_pl8_report(extracted->pl8);
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

struct LoadedPackContainer {
  std::vector<std::uint8_t> bytes;
  romulus::data::Win95PackContainerResource container;
};

[[nodiscard]] std::optional<LoadedPackContainer> load_pack_container(const std::filesystem::path& data_root,
                                                                     const std::string& container_file_arg) {
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

  return LoadedPackContainer{
      .bytes = loaded.value.value().bytes,
      .container = parsed_container.value.value(),
  };
}

int run_pack_text_batch_probe(const std::filesystem::path& data_root,
                              const std::string& container_file_arg,
                              const bool include_all_entries) {
  const auto loaded = load_pack_container(data_root, container_file_arg);
  if (!loaded.has_value()) {
    return 1;
  }

  const auto batch_result = romulus::data::analyze_win95_pack_text_batch(loaded->bytes, loaded->container, 80);
  std::cout << romulus::data::format_win95_pack_text_batch_report(
      batch_result,
      container_file_arg,
      romulus::data::Win95PackTextBatchReportOptions{
          .preview_entry_limit = 8,
          .include_all_entries = include_all_entries,
      });
  return 0;
}

int run_pack_ilbm_index(const std::filesystem::path& data_root,
                        const std::string& container_file_arg,
                        const bool include_all_entries) {
  const auto loaded = load_pack_container(data_root, container_file_arg);
  if (!loaded.has_value()) {
    return 1;
  }

  const auto batch_result = romulus::data::analyze_win95_pack_ilbm_batch(loaded->bytes, loaded->container);
  const auto index = romulus::data::build_win95_pack_ilbm_success_index(batch_result);
  std::cout << romulus::data::format_win95_pack_ilbm_index_report(
      index,
      container_file_arg,
      romulus::data::Win95PackIlbmIndexReportOptions{
          .preview_entry_limit = 8,
          .include_all_entries = include_all_entries,
      });
  return 0;
}

int run_pack_text_index(const std::filesystem::path& data_root,
                        const std::string& container_file_arg,
                        const bool include_all_entries) {
  const auto loaded = load_pack_container(data_root, container_file_arg);
  if (!loaded.has_value()) {
    return 1;
  }

  const auto batch_result = romulus::data::analyze_win95_pack_text_batch(loaded->bytes, loaded->container, 80);
  const auto index = romulus::data::build_win95_pack_text_success_index(batch_result);
  std::cout << romulus::data::format_win95_pack_text_index_report(
      index,
      container_file_arg,
      romulus::data::Win95PackTextIndexReportOptions{
          .preview_entry_limit = 8,
          .include_all_entries = include_all_entries,
      });
  return 0;
}

int run_pack_known_index(const std::filesystem::path& data_root,
                         const std::string& container_file_arg,
                         const bool include_all_entries) {
  const auto loaded = load_pack_container(data_root, container_file_arg);
  if (!loaded.has_value()) {
    return 1;
  }

  const auto index = romulus::data::build_win95_pack_unified_success_index(loaded->bytes, loaded->container, 80);
  std::cout << romulus::data::format_win95_pack_unified_success_index_report(
      index,
      container_file_arg,
      romulus::data::Win95PackUnifiedSuccessReportOptions{
          .preview_entry_limit = 8,
          .include_all_entries = include_all_entries,
      });
  return 0;
}

int run_pack_ilbm_success_export(const std::filesystem::path& data_root,
                                 const std::string& container_file_arg,
                                 const std::size_t entry_index,
                                 const std::string& output_file_arg) {
  const auto loaded = load_pack_container(data_root, container_file_arg);
  if (!loaded.has_value()) {
    return 1;
  }

  const auto batch_result = romulus::data::analyze_win95_pack_ilbm_batch(loaded->bytes, loaded->container);
  const auto index = romulus::data::build_win95_pack_ilbm_success_index(batch_result);
  if (entry_index >= loaded->container.entries.size()) {
    romulus::core::log_error("Invalid PACK entry index " + std::to_string(entry_index) + "; entry count=" +
                             std::to_string(loaded->container.entries.size()));
    return 1;
  }

  const auto indexed_entry = romulus::data::find_win95_pack_ilbm_index_entry(index, entry_index);
  if (!indexed_entry.has_value()) {
    romulus::core::log_error("PACK entry " + std::to_string(entry_index) +
                             " is not in successful ILBM index; use --index-pack-ilbm to inspect candidates.");
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_ilbm_entry(loaded->bytes, loaded->container, entry_index);
  if (!extracted.ok()) {
    romulus::core::log_error(extracted.error.value().message);
    return 1;
  }

  const auto rgba = romulus::data::convert_ilbm_to_rgba(extracted.value->ilbm);
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

  romulus::core::log_info("Exported successful PACK ILBM entry " + std::to_string(entry_index) +
                          " to: " + output_path.string());
  return 0;
}

int run_pack_text_success_export(const std::filesystem::path& data_root,
                                 const std::string& container_file_arg,
                                 const std::size_t entry_index,
                                 const std::string& output_file_arg) {
  const auto loaded = load_pack_container(data_root, container_file_arg);
  if (!loaded.has_value()) {
    return 1;
  }

  const auto batch_result = romulus::data::analyze_win95_pack_text_batch(loaded->bytes, loaded->container, 80);
  const auto index = romulus::data::build_win95_pack_text_success_index(batch_result);
  if (entry_index >= loaded->container.entries.size()) {
    romulus::core::log_error("Invalid PACK entry index " + std::to_string(entry_index) + "; entry count=" +
                             std::to_string(loaded->container.entries.size()));
    return 1;
  }

  const auto indexed_entry = romulus::data::find_win95_pack_text_index_entry(index, entry_index);
  if (!indexed_entry.has_value()) {
    romulus::core::log_error("PACK entry " + std::to_string(entry_index) +
                             " is not in successful text index; use --index-pack-text to inspect candidates.");
    return 1;
  }

  const auto extracted = romulus::data::extract_win95_pack_text_entry(loaded->bytes, loaded->container, entry_index);
  if (!extracted.ok()) {
    romulus::core::log_error(extracted.error.value().message);
    return 1;
  }

  const auto output_path = resolve_data_relative(data_root, output_file_arg);
  std::ofstream output(output_path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!output.is_open()) {
    romulus::core::log_error("Failed to open PACK text export output file: " + output_path.string());
    return 1;
  }

  output.write(extracted.value->decoded_text.data(), static_cast<std::streamsize>(extracted.value->decoded_text.size()));
  if (!output.good()) {
    romulus::core::log_error("Failed to write PACK text export output file: " + output_path.string());
    return 1;
  }

  romulus::core::log_info("Exported successful PACK text entry " + std::to_string(entry_index) +
                          " to: " + output_path.string());
  return 0;
}

int run_pack_text_first_export(const std::filesystem::path& data_root,
                               const std::string& container_file_arg,
                               const std::size_t count,
                               const std::string& output_dir_arg) {
  const auto loaded = load_pack_container(data_root, container_file_arg);
  if (!loaded.has_value()) {
    return 1;
  }

  const auto output_dir = resolve_data_relative(data_root, output_dir_arg);
  std::error_code directory_error;
  std::filesystem::create_directories(output_dir, directory_error);
  if (directory_error) {
    romulus::core::log_error("Failed to prepare export output directory: " + output_dir.string());
    return 1;
  }

  const auto batch_result = romulus::data::analyze_win95_pack_text_batch(loaded->bytes, loaded->container, 80);
  const auto index = romulus::data::build_win95_pack_text_success_index(batch_result);
  const auto export_count = std::min<std::size_t>(count, index.successful_entries.size());

  std::size_t success_count = 0;
  for (std::size_t current = 0; current < export_count; ++current) {
    const auto& index_entry = index.successful_entries[current];
    bool success = false;
    std::string reason = "unknown";
    std::filesystem::path output_path;

    const auto extracted = romulus::data::extract_win95_pack_text_entry(loaded->bytes, loaded->container, index_entry.entry_index);
    if (!extracted.ok()) {
      reason = extracted.error->message;
    } else {
      output_path = output_dir / ("pack_text_entry_" + std::to_string(index_entry.entry_index) + ".txt");
      std::ofstream output(output_path, std::ios::out | std::ios::binary | std::ios::trunc);
      if (!output.is_open()) {
        reason = "failed to open output file";
      } else {
        output.write(extracted.value->decoded_text.data(),
                     static_cast<std::streamsize>(extracted.value->decoded_text.size()));
        if (!output.good()) {
          reason = "failed to write output file";
        } else {
          success = true;
        }
      }
    }

    std::cout << "export_result: entry_index=" << index_entry.entry_index
              << " status=" << (success ? "ok" : "failed");
    if (success) {
      ++success_count;
      std::cout << " output=" << output_path.string();
    } else {
      std::cout << " reason=" << reason;
    }
    std::cout << "\n";
  }

  std::cout << "export_summary: requested=" << export_count << " success=" << success_count
            << " failed=" << (export_count - success_count) << "\n";
  return success_count == export_count ? 0 : 1;
}

int run_pack_ilbm_first_export(const std::filesystem::path& data_root,
                               const std::string& container_file_arg,
                               const std::size_t count,
                               const std::string& output_dir_arg) {
  const auto loaded = load_pack_container(data_root, container_file_arg);
  if (!loaded.has_value()) {
    return 1;
  }

  const auto output_dir = resolve_data_relative(data_root, output_dir_arg);
  std::error_code directory_error;
  std::filesystem::create_directories(output_dir, directory_error);
  if (directory_error) {
    romulus::core::log_error("Failed to prepare export output directory: " + output_dir.string());
    return 1;
  }

  const auto batch_result = romulus::data::analyze_win95_pack_ilbm_batch(loaded->bytes, loaded->container);
  const auto index = romulus::data::build_win95_pack_ilbm_success_index(batch_result);
  const auto export_count = std::min<std::size_t>(count, index.successful_entries.size());

  std::vector<romulus::data::Win95PackIlbmExportResult> results;
  results.reserve(export_count);

  for (std::size_t current = 0; current < export_count; ++current) {
    const auto& index_entry = index.successful_entries[current];
    romulus::data::Win95PackIlbmExportResult result{
        .requested_entry_index = index_entry.entry_index,
        .success = false,
    };

    const auto extracted =
        romulus::data::extract_win95_pack_ilbm_entry(loaded->bytes, loaded->container, index_entry.entry_index);
    if (!extracted.ok()) {
      result.failure_reason = extracted.error->message;
      results.push_back(std::move(result));
      continue;
    }

    const auto rgba = romulus::data::convert_ilbm_to_rgba(extracted.value->ilbm);
    if (!rgba.ok()) {
      result.failure_reason = rgba.error->message;
      results.push_back(std::move(result));
      continue;
    }

    const auto filename = "pack_ilbm_entry_" + std::to_string(index_entry.entry_index) + ".ppm";
    const auto output_path = output_dir / filename;
    const auto exported = romulus::data::export_rgba_image_as_ppm(rgba.value.value(), output_path);
    if (!exported.ok()) {
      result.failure_reason = exported.error->message;
      results.push_back(std::move(result));
      continue;
    }

    result.success = true;
    result.output_path = output_path;
    results.push_back(std::move(result));
  }

  std::size_t success_count = 0;
  for (const auto& result : results) {
    std::cout << "export_result: entry_index=" << result.requested_entry_index
              << " status=" << (result.success ? "ok" : "failed");
    if (result.success) {
      ++success_count;
      std::cout << " output=" << result.output_path->string();
    } else {
      std::cout << " reason=" << result.failure_reason.value_or("unknown");
    }
    std::cout << "\n";
  }

  std::cout << "export_summary: requested=" << export_count << " success=" << success_count
            << " failed=" << (results.size() - success_count) << "\n";
  return success_count == export_count ? 0 : 1;
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

int run_pack_text_export(const std::filesystem::path& data_root,
                         const std::string& container_file_arg,
                         const std::size_t entry_index,
                         const std::string& output_file_arg) {
  const auto extracted = decode_pack_text_entry(data_root, container_file_arg, entry_index);
  if (!extracted.has_value()) {
    return 1;
  }

  const auto output_path = resolve_data_relative(data_root, output_file_arg);
  std::ofstream output(output_path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!output.is_open()) {
    romulus::core::log_error("Failed to open PACK text export output file: " + output_path.string());
    return 1;
  }

  output.write(extracted->decoded_text.data(), static_cast<std::streamsize>(extracted->decoded_text.size()));
  if (!output.good()) {
    romulus::core::log_error("Failed to write PACK text export output file: " + output_path.string());
    return 1;
  }

  romulus::core::log_info("Exported PACK text entry to: " + output_path.string());
  return 0;
}

int run_pack_pl8_export(const std::filesystem::path& data_root,
                        const std::string& container_file_arg,
                        const std::size_t entry_index,
                        const std::string& output_file_arg) {
  const auto extracted = decode_pack_pl8_entry(data_root, container_file_arg, entry_index);
  if (!extracted.has_value()) {
    return 1;
  }

  const auto output_path = resolve_data_relative(data_root, output_file_arg);
  std::ofstream output(output_path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!output.is_open()) {
    romulus::core::log_error("Failed to open PACK PL8 export output file: " + output_path.string());
    return 1;
  }

  output.write(reinterpret_cast<const char*>(extracted->payload_bytes.data()),
               static_cast<std::streamsize>(extracted->payload_bytes.size()));
  if (!output.good()) {
    romulus::core::log_error("Failed to write PACK PL8 export output file: " + output_path.string());
    return 1;
  }

  romulus::core::log_info("Exported PACK PL8 entry to: " + output_path.string());
  return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
  const auto parsed = parse_arguments(argc, argv);
  if (!parsed.has_value()) {
    romulus::core::log_error(
        "Usage: caesar2 [--smoke-test] [--data-dir <path>] [--inventory-manifest] [--manifest-out <path>] "
        "[--probe-file <path>] [--probe-candidate <path>] [--match-signature <path>] "
        "[--probe-lbm <path>] [--probe-pl8 <path> ...] [--probe-pl8-image <path>] "
        "[--probe-pl8-sprites <path>] "
        "[--probe-pl8-image-variant <path>] [--probe-pl8-structured <path>] "
        "[--probe-pl8-structured-regions <path>] "
        "[--compare-pl8-image-variants <lhs_imagepl8> <rhs_imagepl8>] "
        "[--compare-pl8-structured-regions <lhs_imagepl8> <rhs_imagepl8>] "
        "[--probe-256 <path> [--width <w> --height <h>]] "
        "[--view-256-pl8 <image256> <palettepl8> [--width <w> --height <h>]] "
        "[--export-256-pl8 <image256> <palettepl8> --export-output <path> [--width <w> --height <h>]] "
        "[--view-pl8-image-pair <imagepl8> <palette256>] "
        "[--export-pl8-image-pair <imagepl8> <palette256> --export-output <path>] "
        "[--view-pl8-sprite-pair <imagepl8> <palette256>] "
        "[--export-pl8-sprite-pair <imagepl8> <palette256> --export-output <path>] "
        "[--view-pl8-structured-pair <imagepl8> <palette256>] "
        "[--export-pl8-structured-pair <imagepl8> <palette256> --export-output <path>] "
        "[--probe-exe <path>] "
        "[--probe-exe-resources <path>] "
        "[--probe-exe-resource-payloads <path>] "
        "[--probe-win95-data] "
        "[--probe-win95-container <path>] "
        "[--probe-win95-container-entries-all] "
        "[--probe-pack-ilbm <container> <entry_index>] "
        "[--probe-pack-text <container> <entry_index>] "
        "[--probe-pack-pl8 <container> <entry_index>] "
        "[--probe-pack-text-batch <container>] "
        "[--probe-pack-text-batch-entries-all] "
        "[--probe-pack-ilbm-batch <container>] "
        "[--probe-pack-ilbm-batch-entries-all] "
        "[--index-pack-text <container>] "
        "[--index-pack-text-entries-all] "
        "[--index-pack-ilbm <container>] "
        "[--index-pack-ilbm-entries-all] "
        "[--index-pack-known <container>] "
        "[--index-pack-known-entries-all] "
        "[--export-pack-text-success <container> <entry_index> --export-output <path>] "
        "[--export-pack-text-first <container> <count> --export-output-dir <dir>] "
        "[--export-pack-ilbm-success <container> <entry_index> --export-output <path>] "
        "[--export-pack-ilbm-first <container> <count> --export-output-dir <dir>] "
        "[--extract-pack-ilbm <container> <entry_index> --export-output <path>] "
        "[--view-pack-ilbm <container> <entry_index>] "
        "[--export-pack-text <container> <entry_index> --export-output <path>] "
        "[--export-pack-pl8 <container> <entry_index> --export-output <path>] "
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
                                         parsed->probe_pl8_image_file.has_value() ||
                                         parsed->probe_pl8_sprites_file.has_value() ||
                                         parsed->probe_pl8_image_variant_file.has_value() ||
                                         parsed->probe_pl8_structured_file.has_value() ||
                                         parsed->probe_pl8_structured_regions_file.has_value() ||
                                         parsed->compare_pl8_image_variant_lhs_file.has_value() ||
                                         parsed->compare_pl8_image_variant_rhs_file.has_value() ||
                                         parsed->compare_pl8_structured_regions_lhs_file.has_value() ||
                                         parsed->compare_pl8_structured_regions_rhs_file.has_value() ||
                                         parsed->probe_256_file.has_value() || parsed->view_256_file.has_value() ||
                                         parsed->export_256_file.has_value() ||
                                         parsed->view_pl8_image_file.has_value() ||
                                         parsed->export_pl8_image_file.has_value() ||
                                         parsed->view_pl8_sprite_file.has_value() ||
                                         parsed->export_pl8_sprite_file.has_value() ||
                                         parsed->view_pl8_structured_file.has_value() ||
                                         parsed->export_pl8_structured_file.has_value() ||
                                         parsed->probe_exe_file.has_value() || parsed->probe_exe_resources_file.has_value() ||
                                         parsed->probe_exe_resource_payloads_file.has_value() ||
                                         parsed->probe_win95_data || parsed->probe_win95_container_file.has_value() ||
                                         parsed->probe_pack_ilbm_container_file.has_value() ||
                                         parsed->probe_pack_text_container_file.has_value() ||
                                         parsed->probe_pack_pl8_container_file.has_value() ||
                                         parsed->probe_pack_text_batch_container_file.has_value() ||
                                         parsed->probe_pack_ilbm_batch_container_file.has_value() ||
                                         parsed->index_pack_text_container_file.has_value() ||
                                         parsed->index_pack_ilbm_container_file.has_value() ||
                                         parsed->index_pack_known_container_file.has_value() ||
                                         parsed->export_pack_text_success_container_file.has_value() ||
                                         parsed->export_pack_text_first_container_file.has_value() ||
                                         parsed->export_pack_ilbm_success_container_file.has_value() ||
                                         parsed->export_pack_ilbm_first_container_file.has_value() ||
                                         parsed->extract_pack_ilbm_container_file.has_value() ||
                                         parsed->view_pack_ilbm_container_file.has_value() ||
                                         parsed->export_pack_text_container_file.has_value() ||
                                         parsed->export_pack_pl8_container_file.has_value() ||
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

  if (parsed->probe_256_file.has_value()) {
    return run_256_probe(data_root, parsed->probe_256_file.value(), parsed->image_width, parsed->image_height);
  }

  if (parsed->probe_pl8_image_file.has_value()) {
    return run_pl8_image_probe(data_root, parsed->probe_pl8_image_file.value());
  }
  if (parsed->probe_pl8_sprites_file.has_value()) {
    return run_pl8_sprite_probe(data_root, parsed->probe_pl8_sprites_file.value());
  }

  if (parsed->probe_pl8_image_variant_file.has_value()) {
    return run_pl8_image_variant_probe(data_root, parsed->probe_pl8_image_variant_file.value());
  }

  if (parsed->probe_pl8_structured_file.has_value()) {
    return run_pl8_structured_probe(data_root, parsed->probe_pl8_structured_file.value());
  }

  if (parsed->probe_pl8_structured_regions_file.has_value()) {
    return run_pl8_structured_regions_probe(data_root, parsed->probe_pl8_structured_regions_file.value());
  }

  if (parsed->compare_pl8_image_variant_lhs_file.has_value()) {
    return run_pl8_image_variant_compare(data_root,
                                         parsed->compare_pl8_image_variant_lhs_file.value(),
                                         parsed->compare_pl8_image_variant_rhs_file.value());
  }

  if (parsed->compare_pl8_structured_regions_lhs_file.has_value()) {
    return run_pl8_structured_regions_compare(data_root,
                                              parsed->compare_pl8_structured_regions_lhs_file.value(),
                                              parsed->compare_pl8_structured_regions_rhs_file.value());
  }

  if (parsed->view_256_file.has_value()) {
    return run_256_pl8_viewer(data_root,
                              parsed->view_256_file.value(),
                              parsed->view_256_palette_file.value(),
                              parsed->image_width,
                              parsed->image_height,
                              parsed->smoke_test,
                              parsed->index_zero_transparent);
  }

  if (parsed->export_256_file.has_value()) {
    return run_256_pl8_export(data_root,
                              parsed->export_256_file.value(),
                              parsed->export_256_palette_file.value(),
                              parsed->image_width,
                              parsed->image_height,
                              parsed->export_output_file.value(),
                              parsed->index_zero_transparent);
  }

  if (parsed->view_pl8_image_file.has_value()) {
    return run_pl8_image_pair_viewer(data_root,
                                     parsed->view_pl8_image_file.value(),
                                     parsed->view_pl8_image_palette_file.value(),
                                     parsed->smoke_test,
                                     parsed->index_zero_transparent);
  }

  if (parsed->export_pl8_image_file.has_value()) {
    return run_pl8_image_pair_export(data_root,
                                     parsed->export_pl8_image_file.value(),
                                     parsed->export_pl8_image_palette_file.value(),
                                     parsed->export_output_file.value(),
                              parsed->index_zero_transparent);
  }

  if (parsed->view_pl8_sprite_file.has_value()) {
    return run_pl8_sprite_pair_viewer(data_root,
                                      parsed->view_pl8_sprite_file.value(),
                                      parsed->view_pl8_sprite_palette_file.value(),
                                      parsed->smoke_test,
                                      parsed->index_zero_transparent);
  }

  if (parsed->export_pl8_sprite_file.has_value()) {
    return run_pl8_sprite_pair_export(data_root,
                                      parsed->export_pl8_sprite_file.value(),
                                      parsed->export_pl8_sprite_palette_file.value(),
                                      parsed->export_output_file.value(),
                                      parsed->index_zero_transparent);
  }

  if (parsed->view_pl8_structured_file.has_value()) {
    return run_pl8_structured_pair_viewer(data_root,
                                          parsed->view_pl8_structured_file.value(),
                                          parsed->view_pl8_structured_palette_file.value(),
                                          parsed->smoke_test,
                                          parsed->index_zero_transparent);
  }

  if (parsed->export_pl8_structured_file.has_value()) {
    return run_pl8_structured_pair_export(data_root,
                                          parsed->export_pl8_structured_file.value(),
                                          parsed->export_pl8_structured_palette_file.value(),
                                          parsed->export_output_file.value(),
                                          parsed->index_zero_transparent);
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

  if (parsed->probe_pack_text_container_file.has_value()) {
    return run_pack_text_probe(data_root,
                               parsed->probe_pack_text_container_file.value(),
                               parsed->probe_pack_text_entry_index.value());
  }

  if (parsed->probe_pack_pl8_container_file.has_value()) {
    return run_pack_pl8_probe(data_root,
                              parsed->probe_pack_pl8_container_file.value(),
                              parsed->probe_pack_pl8_entry_index.value());
  }

  if (parsed->probe_pack_ilbm_batch_container_file.has_value()) {
    return run_pack_ilbm_batch_probe(data_root,
                                     parsed->probe_pack_ilbm_batch_container_file.value(),
                                     parsed->probe_pack_ilbm_batch_entries_all);
  }

  if (parsed->probe_pack_text_batch_container_file.has_value()) {
    return run_pack_text_batch_probe(data_root,
                                     parsed->probe_pack_text_batch_container_file.value(),
                                     parsed->probe_pack_text_batch_entries_all);
  }

  if (parsed->index_pack_text_container_file.has_value()) {
    return run_pack_text_index(data_root,
                               parsed->index_pack_text_container_file.value(),
                               parsed->index_pack_text_entries_all);
  }

  if (parsed->index_pack_ilbm_container_file.has_value()) {
    return run_pack_ilbm_index(data_root,
                               parsed->index_pack_ilbm_container_file.value(),
                               parsed->index_pack_ilbm_entries_all);
  }

  if (parsed->index_pack_known_container_file.has_value()) {
    return run_pack_known_index(data_root,
                                parsed->index_pack_known_container_file.value(),
                                parsed->index_pack_known_entries_all);
  }

  if (parsed->export_pack_ilbm_success_container_file.has_value()) {
    return run_pack_ilbm_success_export(data_root,
                                        parsed->export_pack_ilbm_success_container_file.value(),
                                        parsed->export_pack_ilbm_success_entry_index.value(),
                                        parsed->export_output_file.value());
  }

  if (parsed->export_pack_text_success_container_file.has_value()) {
    return run_pack_text_success_export(data_root,
                                        parsed->export_pack_text_success_container_file.value(),
                                        parsed->export_pack_text_success_entry_index.value(),
                                        parsed->export_output_file.value());
  }

  if (parsed->export_pack_text_first_container_file.has_value()) {
    return run_pack_text_first_export(data_root,
                                      parsed->export_pack_text_first_container_file.value(),
                                      parsed->export_pack_text_first_count.value(),
                                      parsed->export_output_dir.value());
  }

  if (parsed->export_pack_ilbm_first_container_file.has_value()) {
    return run_pack_ilbm_first_export(data_root,
                                      parsed->export_pack_ilbm_first_container_file.value(),
                                      parsed->export_pack_ilbm_first_count.value(),
                                      parsed->export_output_dir.value());
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

  if (parsed->export_pack_text_container_file.has_value()) {
    return run_pack_text_export(data_root,
                                parsed->export_pack_text_container_file.value(),
                                parsed->export_pack_text_entry_index.value(),
                                parsed->export_output_file.value());
  }

  if (parsed->export_pack_pl8_container_file.has_value()) {
    return run_pack_pl8_export(data_root,
                               parsed->export_pack_pl8_container_file.value(),
                               parsed->export_pack_pl8_entry_index.value(),
                               parsed->export_output_file.value());
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
