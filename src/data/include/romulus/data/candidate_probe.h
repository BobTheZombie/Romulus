#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace romulus::data {

enum class CandidateFileKind {
  TextLike,
  MzExecutableLike,
  OpaqueBinary,
};

struct CandidateTextPreview {
  std::size_t bytes_previewed = 0;
  bool truncated = false;
  std::string text;
};

struct CandidateFileReport {
  std::filesystem::path relative_path;
  std::size_t size_bytes = 0;
  std::string signature_hex;
  std::string signature_ascii;
  std::optional<std::uint16_t> first_u16_le;
  std::optional<std::uint32_t> first_u32_le;
  CandidateFileKind kind = CandidateFileKind::OpaqueBinary;
  std::optional<CandidateTextPreview> text_preview;
};

struct CandidateDatIxPairReport {
  std::filesystem::path dat_relative_path;
  std::filesystem::path ix_relative_path;
  std::size_t dat_size_bytes = 0;
  std::size_t ix_size_bytes = 0;
  std::optional<std::uint32_t> dat_first_u32_le;
  std::optional<std::uint32_t> ix_first_u32_le;
};

struct CandidateProbeBundleReport {
  std::vector<CandidateFileReport> files;
  std::vector<CandidateDatIxPairReport> dat_ix_pairs;
};

struct CandidateProbeError {
  std::filesystem::path requested_path;
  std::string message;
};

struct ProbeCandidatesResult {
  std::optional<CandidateProbeBundleReport> value;
  std::optional<CandidateProbeError> error;

  [[nodiscard]] bool ok() const {
    return value.has_value();
  }
};

constexpr std::size_t k_default_candidate_probe_max_bytes = 8 * 1024 * 1024;

[[nodiscard]] ProbeCandidatesResult probe_candidate_files(const std::filesystem::path& data_root,
                                                          const std::vector<std::string>& candidates,
                                                          std::size_t max_file_load_bytes =
                                                              k_default_candidate_probe_max_bytes);
[[nodiscard]] std::string format_candidate_probe_report(const CandidateProbeBundleReport& report);

}  // namespace romulus::data
