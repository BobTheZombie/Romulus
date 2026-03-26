#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace romulus::platform {

struct BootstrapAssetSelection {
  std::filesystem::path absolute_path;
  std::filesystem::path logical_path;
  bool used_override = false;
};

[[nodiscard]] std::vector<std::filesystem::path> default_bootstrap_asset_candidates();

[[nodiscard]] std::optional<BootstrapAssetSelection> select_bootstrap_asset(
    const std::filesystem::path& data_root,
    const std::optional<std::filesystem::path>& override_path = std::nullopt);

}  // namespace romulus::platform
