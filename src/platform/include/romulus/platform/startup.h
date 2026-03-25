#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "romulus/data/data_root.h"

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

enum class SetupWizardState {
  WizardWelcome,
  WizardChooseFolder,
  WizardValidating,
  WizardValidationFailed,
  WizardConfirm,
  WizardDone,
};

enum class SetupWizardAction {
  Continue,
  ChooseFolder,
  Retry,
  Confirm,
  Exit,
};

struct SetupWizardSnapshot {
  SetupWizardState state = SetupWizardState::WizardWelcome;
  std::optional<std::filesystem::path> candidate_root;
  std::vector<data::RequiredEntry> missing_required_entries;
  std::vector<data::RequiredEntry> missing_optional_entries;
  std::string summary;
  bool picker_canceled = false;
};

using SetupWizardPrompt = std::function<SetupWizardAction(const SetupWizardSnapshot& snapshot)>;

struct SetupWizardResult {
  bool completed = false;
  std::optional<std::filesystem::path> data_root;
};

[[nodiscard]] SetupWizardResult run_setup_wizard(const std::filesystem::path& config_path,
                                                 const FolderPicker& folder_picker,
                                                 const SetupWizardPrompt& prompt,
                                                 const StartupStatus& initial_status = {});

[[nodiscard]] std::string format_required_entries_summary(const std::vector<data::RequiredEntry>& entries);
[[nodiscard]] std::string format_optional_entries_warning(const std::vector<data::RequiredEntry>& entries);

}  // namespace romulus::platform
