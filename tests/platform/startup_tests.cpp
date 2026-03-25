#include "romulus/platform/startup.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "romulus/data/data_root.h"

namespace {

int assert_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    return 1;
  }

  return 0;
}

std::filesystem::path make_temp_dir(const std::string& suffix) {
  const auto root = std::filesystem::temp_directory_path();
  const auto unique_name = "romulus-startup-tests-" + suffix + "-" + std::to_string(std::rand());
  const auto path = root / unique_name;
  std::filesystem::create_directories(path);
  return path;
}

void write_file(const std::filesystem::path& path) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path);
  stream << "test";
}

void create_valid_win95_layout(const std::filesystem::path& root) {
  for (const auto& entry : romulus::data::required_entries()) {
    const auto target = root / entry.relative_path;
    if (entry.type == romulus::data::RequiredEntryType::Directory) {
      std::filesystem::create_directories(target);
    } else {
      write_file(target);
    }
  }
}

int test_startup_without_data_root_enters_setup_state() {
  const auto status = romulus::platform::evaluate_startup_data_root(std::nullopt);
  return assert_true(status.state == romulus::platform::StartupState::NoDataRootConfigured,
                     "missing configured root should enter setup state");
}

int test_invalid_selected_root_stays_invalid() {
  const auto root = make_temp_dir("invalid-selected-root");
  const auto status = romulus::platform::evaluate_startup_data_root(root);

  const int result = assert_true(status.state == romulus::platform::StartupState::DataRootInvalid,
                                 "missing required files should keep startup invalid");
  std::filesystem::remove_all(root);
  return result;
}

int test_valid_selected_root_transitions_to_ready() {
  const auto root = make_temp_dir("valid-selected-root");
  create_valid_win95_layout(root);

  const auto status = romulus::platform::evaluate_startup_data_root(root);
  const int result = assert_true(status.state == romulus::platform::StartupState::DataRootReady,
                                 "valid selected root should be ready");

  std::filesystem::remove_all(root);
  return result;
}

int test_persisted_root_reused_on_next_launch_when_valid() {
  const auto root = make_temp_dir("persisted-root");
  create_valid_win95_layout(root);

  const auto config_dir = make_temp_dir("config");
  const auto config_file = config_dir / "startup.conf";

  if (assert_true(romulus::platform::persist_data_root(config_file, root), "persist should succeed") != 0) {
    std::filesystem::remove_all(root);
    std::filesystem::remove_all(config_dir);
    return 1;
  }

  const auto loaded = romulus::platform::load_persisted_data_root(config_file);
  if (assert_true(loaded.has_value(), "persisted root should be loaded") != 0) {
    std::filesystem::remove_all(root);
    std::filesystem::remove_all(config_dir);
    return 1;
  }

  const auto status = romulus::platform::evaluate_startup_data_root(*loaded);
  const int result = assert_true(status.state == romulus::platform::StartupState::DataRootReady,
                                 "loaded persisted root should still validate");

  std::filesystem::remove_all(root);
  std::filesystem::remove_all(config_dir);
  return result;
}

int test_wizard_first_launch_reaches_done_on_valid_selection() {
  const auto valid_root = make_temp_dir("wizard-valid");
  create_valid_win95_layout(valid_root);
  const auto config_dir = make_temp_dir("wizard-config");
  const auto config_file = config_dir / "startup.conf";

  int picker_calls = 0;
  auto picker = [&]() -> std::optional<std::filesystem::path> {
    ++picker_calls;
    return valid_root;
  };

  auto prompt = [](const romulus::platform::SetupWizardSnapshot& snapshot) {
    switch (snapshot.state) {
      case romulus::platform::SetupWizardState::WizardWelcome:
        return romulus::platform::SetupWizardAction::Continue;
      case romulus::platform::SetupWizardState::WizardChooseFolder:
        return romulus::platform::SetupWizardAction::ChooseFolder;
      case romulus::platform::SetupWizardState::WizardValidationFailed:
        return romulus::platform::SetupWizardAction::Retry;
      case romulus::platform::SetupWizardState::WizardConfirm:
        return romulus::platform::SetupWizardAction::Confirm;
      case romulus::platform::SetupWizardState::WizardDone:
      case romulus::platform::SetupWizardState::WizardValidating:
        return romulus::platform::SetupWizardAction::Continue;
    }

    return romulus::platform::SetupWizardAction::Exit;
  };

  const auto result = romulus::platform::run_setup_wizard(config_file, picker, prompt);
  const auto persisted = romulus::platform::load_persisted_data_root(config_file);

  int rc = 0;
  rc |= assert_true(result.completed, "wizard should complete on valid path");
  rc |= assert_true(result.data_root.has_value(), "wizard should return selected path");
  rc |= assert_true(picker_calls == 1, "folder picker should be called once");
  rc |= assert_true(persisted.has_value(), "valid path should be persisted");

  std::filesystem::remove_all(valid_root);
  std::filesystem::remove_all(config_dir);
  return rc;
}

int test_wizard_invalid_selection_shows_failed_state_and_retries() {
  const auto invalid_root = make_temp_dir("wizard-invalid");
  const auto valid_root = make_temp_dir("wizard-retry-valid");
  create_valid_win95_layout(valid_root);
  const auto config_dir = make_temp_dir("wizard-retry-config");
  const auto config_file = config_dir / "startup.conf";

  int picker_calls = 0;
  auto picker = [&]() -> std::optional<std::filesystem::path> {
    ++picker_calls;
    if (picker_calls == 1) {
      return invalid_root;
    }

    return valid_root;
  };

  bool saw_failed = false;
  auto prompt = [&](const romulus::platform::SetupWizardSnapshot& snapshot) {
    if (snapshot.state == romulus::platform::SetupWizardState::WizardValidationFailed) {
      saw_failed = !snapshot.missing_required_entries.empty();
      return romulus::platform::SetupWizardAction::Retry;
    }

    if (snapshot.state == romulus::platform::SetupWizardState::WizardWelcome) {
      return romulus::platform::SetupWizardAction::Continue;
    }

    if (snapshot.state == romulus::platform::SetupWizardState::WizardChooseFolder) {
      return romulus::platform::SetupWizardAction::ChooseFolder;
    }

    if (snapshot.state == romulus::platform::SetupWizardState::WizardConfirm) {
      return romulus::platform::SetupWizardAction::Confirm;
    }

    return romulus::platform::SetupWizardAction::Continue;
  };

  const auto result = romulus::platform::run_setup_wizard(config_file, picker, prompt);

  int rc = 0;
  rc |= assert_true(saw_failed, "wizard should expose missing required entries");
  rc |= assert_true(result.completed, "wizard should allow retry and complete");
  rc |= assert_true(picker_calls == 2, "wizard should retry folder picker after invalid selection");

  std::filesystem::remove_all(invalid_root);
  std::filesystem::remove_all(valid_root);
  std::filesystem::remove_all(config_dir);
  return rc;
}

int test_wizard_canceled_picker_returns_to_choose_state() {
  const auto valid_root = make_temp_dir("wizard-cancel-valid");
  create_valid_win95_layout(valid_root);
  const auto config_dir = make_temp_dir("wizard-cancel-config");
  const auto config_file = config_dir / "startup.conf";

  int picker_calls = 0;
  auto picker = [&]() -> std::optional<std::filesystem::path> {
    ++picker_calls;
    if (picker_calls == 1) {
      return std::nullopt;
    }

    return valid_root;
  };

  bool saw_cancel_feedback = false;
  auto prompt = [&](const romulus::platform::SetupWizardSnapshot& snapshot) {
    if (snapshot.state == romulus::platform::SetupWizardState::WizardChooseFolder && snapshot.picker_canceled) {
      saw_cancel_feedback = true;
    }

    if (snapshot.state == romulus::platform::SetupWizardState::WizardConfirm) {
      return romulus::platform::SetupWizardAction::Confirm;
    }

    if (snapshot.state == romulus::platform::SetupWizardState::WizardWelcome) {
      return romulus::platform::SetupWizardAction::Continue;
    }

    if (snapshot.state == romulus::platform::SetupWizardState::WizardChooseFolder) {
      return romulus::platform::SetupWizardAction::ChooseFolder;
    }

    return romulus::platform::SetupWizardAction::Continue;
  };

  const auto result = romulus::platform::run_setup_wizard(config_file, picker, prompt);

  int rc = 0;
  rc |= assert_true(saw_cancel_feedback, "wizard should report canceled folder selection");
  rc |= assert_true(result.completed, "wizard should recover from canceled folder selection");
  rc |= assert_true(picker_calls == 2, "wizard should allow folder picker retry after cancellation");

  std::filesystem::remove_all(valid_root);
  std::filesystem::remove_all(config_dir);
  return rc;
}

int test_invalid_persisted_root_reenters_wizard() {
  const auto invalid_root = make_temp_dir("wizard-invalid-persisted");
  const auto valid_root = make_temp_dir("wizard-invalid-persisted-valid");
  create_valid_win95_layout(valid_root);
  const auto config_dir = make_temp_dir("wizard-invalid-persisted-config");
  const auto config_file = config_dir / "startup.conf";

  romulus::platform::persist_data_root(config_file, invalid_root);

  const auto loaded = romulus::platform::load_persisted_data_root(config_file);
  if (assert_true(loaded.has_value(), "persisted invalid root should load") != 0) {
    std::filesystem::remove_all(invalid_root);
    std::filesystem::remove_all(valid_root);
    std::filesystem::remove_all(config_dir);
    return 1;
  }

  const auto initial_status = romulus::platform::evaluate_startup_data_root(*loaded);

  int picker_calls = 0;
  auto picker = [&]() -> std::optional<std::filesystem::path> {
    ++picker_calls;
    return valid_root;
  };

  bool saw_invalid_summary = false;
  auto prompt = [&](const romulus::platform::SetupWizardSnapshot& snapshot) {
    if (snapshot.state == romulus::platform::SetupWizardState::WizardWelcome &&
        snapshot.summary.find("invalid") != std::string::npos) {
      saw_invalid_summary = true;
    }

    if (snapshot.state == romulus::platform::SetupWizardState::WizardWelcome) {
      return romulus::platform::SetupWizardAction::Continue;
    }

    if (snapshot.state == romulus::platform::SetupWizardState::WizardChooseFolder) {
      return romulus::platform::SetupWizardAction::ChooseFolder;
    }

    if (snapshot.state == romulus::platform::SetupWizardState::WizardConfirm) {
      return romulus::platform::SetupWizardAction::Confirm;
    }

    return romulus::platform::SetupWizardAction::Continue;
  };

  const auto result = romulus::platform::run_setup_wizard(config_file, picker, prompt, initial_status);

  int rc = 0;
  rc |= assert_true(initial_status.state == romulus::platform::StartupState::DataRootInvalid,
                    "persisted invalid root should evaluate invalid");
  rc |= assert_true(saw_invalid_summary, "wizard should explain invalid persisted root");
  rc |= assert_true(result.completed, "wizard should recover from invalid persisted root");
  rc |= assert_true(picker_calls == 1, "wizard should prompt for new folder when persisted root is invalid");

  std::filesystem::remove_all(invalid_root);
  std::filesystem::remove_all(valid_root);
  std::filesystem::remove_all(config_dir);
  return rc;
}

}  // namespace

int main() {
  if (test_startup_without_data_root_enters_setup_state() != 0) {
    return EXIT_FAILURE;
  }

  if (test_invalid_selected_root_stays_invalid() != 0) {
    return EXIT_FAILURE;
  }

  if (test_valid_selected_root_transitions_to_ready() != 0) {
    return EXIT_FAILURE;
  }

  if (test_persisted_root_reused_on_next_launch_when_valid() != 0) {
    return EXIT_FAILURE;
  }

  if (test_wizard_first_launch_reaches_done_on_valid_selection() != 0) {
    return EXIT_FAILURE;
  }

  if (test_wizard_invalid_selection_shows_failed_state_and_retries() != 0) {
    return EXIT_FAILURE;
  }

  if (test_wizard_canceled_picker_returns_to_choose_state() != 0) {
    return EXIT_FAILURE;
  }

  if (test_invalid_persisted_root_reenters_wizard() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
