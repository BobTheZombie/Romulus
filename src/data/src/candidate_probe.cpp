#include "romulus/data/candidate_probe.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <unordered_map>

#include "romulus/data/binary_probe.h"
#include "romulus/data/file_loader.h"

namespace romulus::data {
namespace {

constexpr std::size_t k_signature_preview_bytes = 12;
constexpr std::size_t k_text_preview_bytes = 256;
constexpr std::size_t k_text_preview_lines = 8;

[[nodiscard]] std::string to_upper_ascii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
    return static_cast<char>(std::toupper(character));
  });
  return value;
}

[[nodiscard]] std::string to_hex_signature(std::span<const std::uint8_t> bytes) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0');

  for (std::size_t index = 0; index < bytes.size(); ++index) {
    if (index != 0) {
      stream << ' ';
    }

    stream << std::setw(2) << static_cast<unsigned int>(bytes[index]);
  }

  return stream.str();
}

[[nodiscard]] std::string to_ascii_signature(std::span<const std::uint8_t> bytes) {
  std::string output;
  output.reserve(bytes.size());

  for (const auto byte : bytes) {
    const auto character = static_cast<unsigned char>(byte);
    if (std::isprint(character) != 0) {
      output.push_back(static_cast<char>(character));
    } else {
      output.push_back('.');
    }
  }

  return output;
}

[[nodiscard]] bool is_text_like_bytes(std::span<const std::uint8_t> bytes) {
  if (bytes.empty()) {
    return false;
  }

  std::size_t printable_or_whitespace_count = 0;
  for (const auto byte : bytes) {
    if (byte == 0) {
      return false;
    }

    const auto character = static_cast<unsigned char>(byte);
    if (std::isprint(character) != 0 || character == '\n' || character == '\r' || character == '\t') {
      ++printable_or_whitespace_count;
    }
  }

  return printable_or_whitespace_count * 100 >= bytes.size() * 90;
}

[[nodiscard]] bool is_obvious_text_preview_candidate(const std::filesystem::path& relative_path) {
  const auto extension = to_upper_ascii(relative_path.extension().string());
  return extension == ".CFG" || extension == ".TXT" || extension == ".INI";
}

[[nodiscard]] CandidateTextPreview make_text_preview(std::span<const std::uint8_t> bytes) {
  CandidateTextPreview preview;
  const auto preview_bytes = std::min(k_text_preview_bytes, bytes.size());
  preview.bytes_previewed = preview_bytes;
  preview.truncated = preview_bytes < bytes.size();

  std::size_t newline_count = 0;
  for (std::size_t index = 0; index < preview_bytes; ++index) {
    const auto character = static_cast<unsigned char>(bytes[index]);
    if (character == '\r') {
      continue;
    }

    if (character == '\n') {
      if (newline_count >= k_text_preview_lines) {
        preview.truncated = true;
        break;
      }

      ++newline_count;
      preview.text.push_back('\n');
      continue;
    }

    if (std::isprint(character) != 0 || character == '\t') {
      preview.text.push_back(static_cast<char>(character));
    } else {
      preview.text.push_back('.');
    }
  }

  return preview;
}

[[nodiscard]] CandidateFileKind classify_candidate(const BinaryProbeReport& probe,
                                                   std::span<const std::uint8_t> bytes) {
  if (probe.dos_mz_header.has_value()) {
    return CandidateFileKind::MzExecutableLike;
  }

  if (is_text_like_bytes(bytes)) {
    return CandidateFileKind::TextLike;
  }

  return CandidateFileKind::OpaqueBinary;
}

[[nodiscard]] std::string kind_to_string(const CandidateFileKind kind) {
  switch (kind) {
    case CandidateFileKind::TextLike:
      return "text-like";
    case CandidateFileKind::MzExecutableLike:
      return "mz-executable-like";
    case CandidateFileKind::OpaqueBinary:
      return "opaque-binary";
  }

  return "opaque-binary";
}

[[nodiscard]] std::string normalize_key(const std::filesystem::path& value) {
  return to_upper_ascii(value.generic_string());
}

}  // namespace

ProbeCandidatesResult probe_candidate_files(const std::filesystem::path& data_root,
                                            const std::vector<std::string>& candidates,
                                            const std::size_t max_file_load_bytes) {
  CandidateProbeBundleReport bundle;
  bundle.files.reserve(candidates.size());

  std::unordered_map<std::string, std::size_t> by_normalized_rel_path;

  for (const auto& candidate : candidates) {
    std::filesystem::path relative_path = std::filesystem::path(candidate).lexically_normal();
    const auto absolute_path = relative_path.is_absolute() ? relative_path : (data_root / relative_path);

    const auto loaded = load_file_to_memory(absolute_path, max_file_load_bytes);
    if (!loaded.ok()) {
      CandidateProbeError error;
      error.requested_path = relative_path;
      error.message = loaded.error.value().message;
      return {.error = error};
    }

    const auto& file = loaded.value.value();
    const auto probe = probe_loaded_binary(file);

    CandidateFileReport report;
    report.relative_path = relative_path;
    report.size_bytes = file.bytes.size();

    const auto signature_size = std::min(k_signature_preview_bytes, file.bytes.size());
    const auto signature_span = std::span<const std::uint8_t>(file.bytes.data(), signature_size);
    report.signature_hex = to_hex_signature(signature_span);
    report.signature_ascii = to_ascii_signature(signature_span);
    report.first_u16_le = probe.first_u16_le;
    report.first_u32_le = probe.first_u32_le;
    report.kind = classify_candidate(probe, std::span<const std::uint8_t>(file.bytes.data(), file.bytes.size()));

    if (report.kind == CandidateFileKind::TextLike || is_obvious_text_preview_candidate(report.relative_path)) {
      report.text_preview = make_text_preview(std::span<const std::uint8_t>(file.bytes.data(), file.bytes.size()));
    }

    by_normalized_rel_path[normalize_key(relative_path)] = bundle.files.size();
    bundle.files.push_back(std::move(report));
  }

  for (const auto& file_report : bundle.files) {
    const auto extension = to_upper_ascii(file_report.relative_path.extension().string());
    if (extension != ".DAT") {
      continue;
    }

    auto ix_path = file_report.relative_path;
    ix_path.replace_extension(".IX");

    const auto ix_it = by_normalized_rel_path.find(normalize_key(ix_path));
    if (ix_it == by_normalized_rel_path.end()) {
      continue;
    }

    const auto& ix_report = bundle.files[ix_it->second];

    CandidateDatIxPairReport pair;
    pair.dat_relative_path = file_report.relative_path;
    pair.ix_relative_path = ix_report.relative_path;
    pair.dat_size_bytes = file_report.size_bytes;
    pair.ix_size_bytes = ix_report.size_bytes;
    pair.dat_first_u32_le = file_report.first_u32_le;
    pair.ix_first_u32_le = ix_report.first_u32_le;
    bundle.dat_ix_pairs.push_back(std::move(pair));
  }

  return {.value = std::move(bundle)};
}

std::string format_candidate_probe_report(const CandidateProbeBundleReport& report) {
  std::ostringstream output;
  output << "# Caesar II Candidate Probe\n";

  for (const auto& file : report.files) {
    output << "\n[file]\n";
    output << "path: " << file.relative_path.generic_string() << "\n";
    output << "size_bytes: " << file.size_bytes << "\n";
    output << "signature_hex: " << (file.signature_hex.empty() ? "(empty)" : file.signature_hex) << "\n";
    output << "signature_ascii: " << (file.signature_ascii.empty() ? "(empty)" : file.signature_ascii) << "\n";

    if (file.first_u16_le.has_value()) {
      output << "first_u16_le: " << file.first_u16_le.value() << "\n";
    } else {
      output << "first_u16_le: unavailable\n";
    }

    if (file.first_u32_le.has_value()) {
      output << "first_u32_le: " << file.first_u32_le.value() << "\n";
    } else {
      output << "first_u32_le: unavailable\n";
    }

    output << "kind: " << kind_to_string(file.kind) << "\n";
    if (file.text_preview.has_value()) {
      output << "text_preview_bytes: " << file.text_preview->bytes_previewed << "\n";
      output << "text_preview_truncated: " << (file.text_preview->truncated ? "yes" : "no") << "\n";
      output << "text_preview:\n";
      output << file.text_preview->text << "\n";
    }
  }

  if (!report.dat_ix_pairs.empty()) {
    output << "\n[dat_ix_pairs]\n";
    for (const auto& pair : report.dat_ix_pairs) {
      output << "pair: " << pair.dat_relative_path.generic_string() << " <-> " << pair.ix_relative_path.generic_string()
             << "\n";
      output << "  dat_size_bytes: " << pair.dat_size_bytes << "\n";
      output << "  ix_size_bytes: " << pair.ix_size_bytes << "\n";
      output << "  dat_first_u32_le: ";
      if (pair.dat_first_u32_le.has_value()) {
        output << pair.dat_first_u32_le.value() << "\n";
      } else {
        output << "unavailable\n";
      }

      output << "  ix_first_u32_le: ";
      if (pair.ix_first_u32_le.has_value()) {
        output << pair.ix_first_u32_le.value() << "\n";
      } else {
        output << "unavailable\n";
      }
    }
  }

  return output.str();
}

}  // namespace romulus::data
