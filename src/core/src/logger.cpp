#include "romulus/core/logger.h"

#include <iostream>

namespace romulus::core {
namespace {

[[nodiscard]] constexpr std::string_view to_string(LogLevel level) {
  switch (level) {
    case LogLevel::Info:
      return "INFO";
    case LogLevel::Warning:
      return "WARN";
    case LogLevel::Error:
      return "ERROR";
  }

  return "UNKNOWN";
}

}  // namespace

void log(LogLevel level, std::string_view message) {
  std::ostream& stream = (level == LogLevel::Error) ? std::cerr : std::cout;
  stream << "[" << to_string(level) << "] " << message << '\n';
}

void log_info(std::string_view message) {
  log(LogLevel::Info, message);
}

void log_warning(std::string_view message) {
  log(LogLevel::Warning, message);
}

void log_error(std::string_view message) {
  log(LogLevel::Error, message);
}

}  // namespace romulus::core
