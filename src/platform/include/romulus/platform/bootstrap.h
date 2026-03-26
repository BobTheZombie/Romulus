#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace romulus::platform {

struct BootstrapAssetSelection {
  std::filesystem::path absolute_path;
  std::filesystem::path logical_path;
  bool used_override = false;
  bool case_insensitive_resolution_attempted = false;
};

struct ForumOverlayAssetSelection {
  std::filesystem::path image256_logical_path;
  std::filesystem::path image256_absolute_path;
  std::filesystem::path palette_pl8_logical_path;
  std::filesystem::path palette_pl8_absolute_path;
  bool case_insensitive_resolution_attempted = false;
};

struct ForumOverlayAssetSpec {
  std::filesystem::path image256_path;
  std::filesystem::path palette_pl8_path;
};

[[nodiscard]] std::vector<std::filesystem::path> default_bootstrap_asset_candidates();

[[nodiscard]] std::optional<BootstrapAssetSelection> select_bootstrap_asset(
    const std::filesystem::path& data_root,
    const std::optional<std::filesystem::path>& override_path = std::nullopt);

[[nodiscard]] std::filesystem::path forum_background_logical_path();
[[nodiscard]] std::vector<ForumOverlayAssetSpec> default_forum_overlay_specs();

[[nodiscard]] std::optional<BootstrapAssetSelection> select_forum_background_asset(
    const std::filesystem::path& data_root);

[[nodiscard]] std::optional<ForumOverlayAssetSelection> select_forum_overlay_asset(
    const std::filesystem::path& data_root,
    const ForumOverlayAssetSpec& spec);

}  // namespace romulus::platform
