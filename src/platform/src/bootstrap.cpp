#include "romulus/platform/bootstrap.h"

#include <array>

#include "romulus/data/path_resolver.h"

namespace romulus::platform {
namespace {

[[nodiscard]] std::optional<BootstrapAssetSelection> resolve_candidate(const std::filesystem::path& data_root,
                                                                        const std::filesystem::path& candidate,
                                                                        const bool used_override) {
  BootstrapAssetSelection selection;
  selection.logical_path = candidate;
  selection.used_override = used_override;

  if (candidate.is_absolute()) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(candidate, error) || error) {
      return std::nullopt;
    }

    selection.absolute_path = candidate;
    selection.case_insensitive_resolution_attempted = false;
    return selection;
  }

  selection.case_insensitive_resolution_attempted = true;
  const auto resolved = romulus::data::resolve_case_insensitive(data_root, candidate);
  if (!resolved.has_value()) {
    return std::nullopt;
  }

  std::error_code error;
  if (!std::filesystem::is_regular_file(*resolved, error) || error) {
    return std::nullopt;
  }

  selection.absolute_path = *resolved;
  return selection;
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
