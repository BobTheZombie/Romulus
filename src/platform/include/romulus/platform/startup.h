#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace romulus::platform {

enum class StartupState {
  NoDataRootConfigured,
  DataRootInvalid,
  DataRootReady,
};

struct StartupStatus {
  StartupState state = StartupState::NoDataRootConfigured;
  std::optional<std::filesystem::path> data_root;
  std::string message;
};

[[nodiscard]] StartupStatus evaluate_startup_data_root(const std::optional<std::filesystem::path>& data_root);

[[nodiscard]] std::filesystem::path default_startup_config_path();
[[nodiscard]] std::optional<std::filesystem::path> load_persisted_data_root(const std::filesystem::path& config_path);
[[nodiscard]] bool persist_data_root(const std::filesystem::path& config_path, const std::filesystem::path& data_root);

using FolderPicker = std::function<std::optional<std::filesystem::path>()>;
[[nodiscard]] std::optional<std::filesystem::path> pick_data_root_with_native_dialog();

}  // namespace romulus::platform
