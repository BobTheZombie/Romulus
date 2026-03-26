#pragma once

#include <filesystem>
#include <optional>

namespace romulus::data {

[[nodiscard]] std::optional<std::filesystem::path> resolve_case_insensitive(const std::filesystem::path& root,
                                                                             const std::filesystem::path& relative_path);

}  // namespace romulus::data
