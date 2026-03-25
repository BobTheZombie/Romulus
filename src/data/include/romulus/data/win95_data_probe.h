#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace romulus::data {

enum class Win95ProbeNodeKind {
  Missing,
  File,
  Directory,
  Other,
};

enum class Win95ProbeFileKind {
  TextLike,
  OpaqueBinary,
  PossibleContainer,
  ExecutableLike,
};

struct Win95ProbeFileDetails {
  std::size_t size_bytes = 0;
  std::string signature_hex;
  std::string signature_ascii;
  std::optional<std::uint16_t> first_u16_le;
  std::optional<std::uint32_t> first_u32_le;
  Win95ProbeFileKind kind = Win95ProbeFileKind::OpaqueBinary;
};

struct Win95DirectoryEntryHint {
  std::string name;
  Win95ProbeNodeKind kind = Win95ProbeNodeKind::Other;
  std::optional<std::size_t> size_bytes;
  std::optional<std::string> signature_hex;
  std::optional<Win95ProbeFileKind> file_kind;
};

struct Win95ProbeDirectoryDetails {
  std::size_t scanned_entry_count = 0;
  bool truncated = false;
  std::vector<Win95DirectoryEntryHint> preview_entries;
};

struct Win95InstallEntryReport {
  std::string entry_name;
  std::filesystem::path relative_path;
  Win95ProbeNodeKind node_kind = Win95ProbeNodeKind::Missing;
  std::optional<Win95ProbeFileDetails> file;
  std::optional<Win95ProbeDirectoryDetails> directory;
};

struct Win95DataPairSummary {
  std::string relationship;
  std::optional<long long> size_delta_bytes;
  std::string hint;
};

struct Win95DataProbeReport {
  std::filesystem::path data_root;
  Win95InstallEntryReport data_entry;
  Win95InstallEntryReport data0_entry;
  Win95DataPairSummary pair_summary;
};

struct Win95DataProbeError {
  std::filesystem::path path;
  std::string message;
};

struct ProbeWin95DataResult {
  std::optional<Win95DataProbeReport> value;
  std::optional<Win95DataProbeError> error;

  [[nodiscard]] bool ok() const {
    return value.has_value();
  }
};

constexpr std::size_t k_default_win95_probe_max_file_load_bytes = 8 * 1024 * 1024;

[[nodiscard]] ProbeWin95DataResult probe_win95_data_entries(const std::filesystem::path& data_root,
                                                            std::size_t max_file_load_bytes =
                                                                k_default_win95_probe_max_file_load_bytes);
[[nodiscard]] std::string format_win95_data_probe_report(const Win95DataProbeReport& report);

}  // namespace romulus::data
