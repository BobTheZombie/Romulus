#include "romulus/platform/startup.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "romulus/data/data_root.h"

namespace romulus::platform {
namespace {

[[nodiscard]] std::optional<std::string> env_value(const char* key) {
  const char* value = std::getenv(key);
  if (value == nullptr || value[0] == '\0') {
    return std::nullopt;
  }

  return std::string(value);
}

[[nodiscard]] std::optional<std::filesystem::path> run_dialog_command(const char* command) {
  std::array<char, 1024> buffer{};
  std::string output;

  FILE* pipe = popen(command, "r");
  if (pipe == nullptr) {
    return std::nullopt;
  }

  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }

  const int status = pclose(pipe);
  if (status != 0 || output.empty()) {
    return std::nullopt;
  }

  while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
    output.pop_back();
  }

  if (output.empty()) {
    return std::nullopt;
  }

  return std::filesystem::path(output);
}

[[nodiscard]] std::vector<data::RequiredEntry> collect_missing_optional_entries(const std::filesystem::path& root) {
  std::vector<data::RequiredEntry> missing;

  std::error_code error;
  for (const auto& entry : data::expected_optional_entries()) {
    const auto candidate = root / entry.relative_path;
    const bool matches_type = entry.type == data::RequiredEntryType::Directory
                                  ? std::filesystem::is_directory(candidate, error)
                                  : std::filesystem::is_regular_file(candidate, error);
    if (!matches_type) {
      missing.push_back(entry);
    }
  }

  return missing;
}

[[nodiscard]] SetupWizardSnapshot make_snapshot(SetupWizardState state) {
  SetupWizardSnapshot snapshot;
  snapshot.state = state;
  return snapshot;
}

}  // namespace

StartupStatus evaluate_startup_data_root(const std::optional<std::filesystem::path>& data_root) {
  StartupStatus status;
  if (!data_root.has_value()) {
    status.state = StartupState::NoDataRootConfigured;
    status.message = "No Caesar II data root is configured.";
    return status;
  }

  const auto validation = romulus::data::validate_data_root(*data_root);
  if (!validation.ok) {
    status.state = StartupState::DataRootInvalid;
    status.message = romulus::data::format_validation_error(validation);
    return status;
  }

  status.state = StartupState::DataRootReady;
  status.data_root = *data_root;
  status.message = "Caesar II data root is ready.";
  return status;
}

std::filesystem::path default_startup_config_path() {
  if (const auto xdg = env_value("XDG_CONFIG_HOME"); xdg.has_value()) {
    return std::filesystem::path(*xdg) / "romulus" / "startup.conf";
  }

  if (const auto home = env_value("HOME"); home.has_value()) {
    return std::filesystem::path(*home) / ".config" / "romulus" / "startup.conf";
  }

  return std::filesystem::path("startup.conf");
}

std::optional<std::filesystem::path> load_persisted_data_root(const std::filesystem::path& config_path) {
  std::ifstream stream(config_path);
  if (!stream.is_open()) {
    return std::nullopt;
  }

  std::string line;
  while (std::getline(stream, line)) {
    if (line.rfind("data_root=", 0) == 0) {
      const std::string value = line.substr(std::string("data_root=").size());
      if (value.empty()) {
        return std::nullopt;
      }

      return std::filesystem::path(value);
    }
  }

  return std::nullopt;
}

bool persist_data_root(const std::filesystem::path& config_path, const std::filesystem::path& data_root) {
  std::error_code error;
  std::filesystem::create_directories(config_path.parent_path(), error);
  if (error) {
    return false;
  }

  std::ofstream stream(config_path, std::ios::trunc);
  if (!stream.is_open()) {
    return false;
  }

  stream << "data_root=" << data_root.string() << '\n';
  return stream.good();
}

std::optional<std::filesystem::path> pick_data_root_with_native_dialog() {
  if (const auto picked = run_dialog_command(
          "zenity --file-selection --directory --title='Select Caesar II Win95 install directory' 2>/dev/null");
      picked.has_value()) {
    return picked;
  }

  if (const auto picked = run_dialog_command(
          "kdialog --getexistingdirectory \"$HOME\" \"Select Caesar II Win95 install directory\" 2>/dev/null");
      picked.has_value()) {
    return picked;
  }

  return std::nullopt;
}

SetupWizardResult run_setup_wizard(const std::filesystem::path& config_path,
                                   const FolderPicker& folder_picker,
                                   const SetupWizardPrompt& prompt,
                                   const StartupStatus& initial_status) {
  SetupWizardResult result;

  SetupWizardState state = SetupWizardState::WizardWelcome;
  SetupWizardSnapshot snapshot = make_snapshot(state);
  if (initial_status.state == StartupState::DataRootInvalid && initial_status.data_root.has_value()) {
    snapshot.summary = initial_status.message;
  }

  std::optional<std::filesystem::path> candidate_root;
  data::DataRootValidationResult validation;
  std::vector<data::RequiredEntry> missing_optional;

  bool running = true;
  while (running) {
    snapshot.state = state;
    snapshot.candidate_root = candidate_root;
    snapshot.missing_required_entries = validation.missing_entries;
    snapshot.missing_optional_entries = missing_optional;

    switch (state) {
      case SetupWizardState::WizardWelcome: {
        if (initial_status.state == StartupState::DataRootInvalid && !initial_status.message.empty()) {
          snapshot.summary = "Configured data root is invalid. Setup is required.\n\n" + initial_status.message;
        } else {
          snapshot.summary =
              "Welcome to Romulus setup.\nSelect your Caesar II Win95 install folder to continue.";
        }

        const auto action = prompt(snapshot);
        if (action == SetupWizardAction::Exit) {
          running = false;
        } else {
          state = SetupWizardState::WizardChooseFolder;
        }
        break;
      }

      case SetupWizardState::WizardChooseFolder: {
        snapshot.summary = "Choose the Caesar II Win95 install folder.";
        if (snapshot.picker_canceled) {
          snapshot.summary += "\n\nFolder selection was canceled. You can retry or exit.";
        }

        const auto action = prompt(snapshot);
        if (action == SetupWizardAction::Exit) {
          running = false;
          break;
        }

        if (action != SetupWizardAction::ChooseFolder) {
          break;
        }

        const auto selected = folder_picker();
        if (!selected.has_value()) {
          snapshot.picker_canceled = true;
          break;
        }

        snapshot.picker_canceled = false;
        candidate_root = data::resolve_data_root(selected->string());
        state = SetupWizardState::WizardValidating;
        break;
      }

      case SetupWizardState::WizardValidating: {
        if (!candidate_root.has_value()) {
          state = SetupWizardState::WizardChooseFolder;
          break;
        }

        validation = data::validate_data_root(*candidate_root);
        missing_optional = collect_missing_optional_entries(*candidate_root);
        state = validation.ok ? SetupWizardState::WizardConfirm : SetupWizardState::WizardValidationFailed;
        break;
      }

      case SetupWizardState::WizardValidationFailed: {
        snapshot.summary = data::format_validation_error(validation);
        const auto action = prompt(snapshot);
        if (action == SetupWizardAction::Exit) {
          running = false;
        } else {
          state = SetupWizardState::WizardChooseFolder;
        }
        break;
      }

      case SetupWizardState::WizardConfirm: {
        if (!candidate_root.has_value()) {
          state = SetupWizardState::WizardChooseFolder;
          break;
        }

        std::ostringstream summary;
        summary << "Selected folder is valid:\n" << candidate_root->string();
        if (!missing_optional.empty()) {
          summary << "\n\nOptional Win95 entries not found:\n" << format_optional_entries_warning(missing_optional);
        }
        summary << "\n\nSave this folder and continue?";
        snapshot.summary = summary.str();

        const auto action = prompt(snapshot);
        if (action == SetupWizardAction::Exit) {
          running = false;
          break;
        }

        if (action == SetupWizardAction::Retry) {
          state = SetupWizardState::WizardChooseFolder;
          break;
        }

        result.completed = true;
        result.data_root = candidate_root;
        persist_data_root(config_path, *candidate_root);
        state = SetupWizardState::WizardDone;
        break;
      }

      case SetupWizardState::WizardDone: {
        snapshot.summary = "Setup complete. Romulus will continue startup.";
        prompt(snapshot);
        running = false;
        break;
      }
    }
  }

  return result;
}

std::string format_required_entries_summary(const std::vector<data::RequiredEntry>& entries) {
  if (entries.empty()) {
    return "(none)";
  }

  std::ostringstream stream;
  for (const auto& entry : entries) {
    stream << "- " << entry.relative_path.string();
    stream << (entry.type == data::RequiredEntryType::Directory ? " (directory)" : " (file)");
    stream << '\n';
  }

  return stream.str();
}

std::string format_optional_entries_warning(const std::vector<data::RequiredEntry>& entries) {
  return format_required_entries_summary(entries);
}

}  // namespace romulus::platform
