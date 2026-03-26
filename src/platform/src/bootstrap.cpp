#include "romulus/platform/bootstrap.h"

#include <array>

namespace romulus::platform {
namespace {

[[nodiscard]] std::optional<BootstrapAssetSelection> resolve_candidate(const std::filesystem::path& data_root,
                                                                        const std::filesystem::path& candidate,
                                                                        const bool used_override) {
  const auto resolved = candidate.is_absolute() ? candidate : (data_root / candidate);
  std::error_code error;
  if (!std::filesystem::is_regular_file(resolved, error) || error) {
    return std::nullopt;
  }

  return BootstrapAssetSelection{
      .absolute_path = resolved,
      .logical_path = candidate,
      .used_override = used_override,
  };
}

}  // namespace

std::vector<std::filesystem::path> default_bootstrap_asset_candidates() {
  return {
      std::filesystem::path("data") / "forum.lbm",
      std::filesystem::path("data") / "empire2.lbm",
      std::filesystem::path("data") / "fading.lbm",
  };
}

std::optional<BootstrapAssetSelection> select_bootstrap_asset(const std::filesystem::path& data_root,
                                                              const std::optional<std::filesystem::path>& override_path) {
  if (override_path.has_value()) {
    if (const auto selected = resolve_candidate(data_root, *override_path, true); selected.has_value()) {
      return selected;
    }
  }

  for (const auto& candidate : default_bootstrap_asset_candidates()) {
    if (const auto selected = resolve_candidate(data_root, candidate, false); selected.has_value()) {
      return selected;
    }
  }

  return std::nullopt;
}

}  // namespace romulus::platform
