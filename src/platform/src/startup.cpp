#include "romulus/platform/startup.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>

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

}  // namespace romulus::platform
