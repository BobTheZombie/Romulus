#include "romulus/data/path_resolver.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace romulus::data {
namespace {

[[nodiscard]] std::string to_lower_ascii(const std::string& value) {
  std::string lowered;
  lowered.reserve(value.size());
  for (const unsigned char ch : value) {
    lowered.push_back(static_cast<char>(std::tolower(ch)));
  }
  return lowered;
}

[[nodiscard]] bool is_valid_segment(const std::string& segment) {
  if (segment.empty()) {
    return false;
  }

  if (segment == "." || segment == "..") {
    return false;
  }

  return segment.find(':') == std::string::npos;
}

[[nodiscard]] std::optional<std::vector<std::string>> split_relative_path(const std::filesystem::path& relative_path) {
  if (relative_path.empty() || relative_path.is_absolute() || relative_path.has_root_directory() ||
      relative_path.has_root_name()) {
    return std::nullopt;
  }

  std::string normalized = relative_path.generic_string();
  std::replace(normalized.begin(), normalized.end(), '\\', '/');

  if (normalized.empty()) {
    return std::nullopt;
  }

  std::vector<std::string> segments;
  std::string current;
  for (const char ch : normalized) {
    if (ch == '/') {
      if (!is_valid_segment(current)) {
        return std::nullopt;
      }

      segments.push_back(current);
      current.clear();
      continue;
    }

    current.push_back(ch);
  }

  if (!is_valid_segment(current)) {
    return std::nullopt;
  }
  segments.push_back(current);

  return segments;
}

[[nodiscard]] std::optional<std::filesystem::directory_entry> select_match(
    const std::filesystem::path& parent,
    const std::string& segment) {
  std::error_code error;
  std::filesystem::directory_iterator it(parent, error);
  if (error) {
    return std::nullopt;
  }

  const std::string segment_lower = to_lower_ascii(segment);
  std::optional<std::filesystem::directory_entry> exact_match;
  std::vector<std::filesystem::directory_entry> case_insensitive_matches;

  for (const auto& entry : it) {
    const std::string name = entry.path().filename().generic_string();
    if (name == segment) {
      exact_match = entry;
      continue;
    }

    if (to_lower_ascii(name) == segment_lower) {
      case_insensitive_matches.push_back(entry);
    }
  }

  if (exact_match.has_value()) {
    return exact_match;
  }

  if (case_insensitive_matches.empty()) {
    return std::nullopt;
  }

  std::sort(case_insensitive_matches.begin(), case_insensitive_matches.end(),
            [](const std::filesystem::directory_entry& lhs, const std::filesystem::directory_entry& rhs) {
              return lhs.path().filename().generic_string() < rhs.path().filename().generic_string();
            });

  return case_insensitive_matches.front();
}

}  // namespace

std::optional<std::filesystem::path> resolve_case_insensitive(const std::filesystem::path& root,
                                                              const std::filesystem::path& relative_path) {
  std::error_code error;
  if (!std::filesystem::is_directory(root, error) || error) {
    return std::nullopt;
  }

  const auto segments = split_relative_path(relative_path);
  if (!segments.has_value()) {
    return std::nullopt;
  }

  std::filesystem::path current = root;
  for (std::size_t i = 0; i < segments->size(); ++i) {
    const auto matched = select_match(current, segments->at(i));
    if (!matched.has_value()) {
      return std::nullopt;
    }

    current = matched->path();

    if (i + 1 < segments->size() && !matched->is_directory(error)) {
      return std::nullopt;
    }

    if (error) {
      return std::nullopt;
    }
  }

  return current;
}

}  // namespace romulus::data
