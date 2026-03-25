#include "romulus/data/win95_data_probe.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <span>
#include <string_view>

#include "romulus/data/binary_probe.h"
#include "romulus/data/file_loader.h"

namespace romulus::data {
namespace {

constexpr std::size_t k_signature_preview_bytes = 12;
constexpr std::size_t k_directory_preview_entries = 32;
constexpr std::size_t k_directory_scan_entry_limit = 256;

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

[[nodiscard]] Win95ProbeFileKind classify_file_kind(const std::filesystem::path& relative_path,
                                                    const BinaryProbeReport& probe,
                                                    std::span<const std::uint8_t> bytes) {
  if (probe.dos_mz_header.has_value()) {
    return Win95ProbeFileKind::ExecutableLike;
  }

  if (is_text_like_bytes(bytes)) {
    return Win95ProbeFileKind::TextLike;
  }

  const auto upper_name = to_upper_ascii(relative_path.filename().string());
  const auto upper_extension = to_upper_ascii(relative_path.extension().string());
  if (upper_name == "DATA" || upper_name == "DATA0" || upper_extension == ".DAT") {
    return Win95ProbeFileKind::PossibleContainer;
  }

  return Win95ProbeFileKind::OpaqueBinary;
}

[[nodiscard]] std::string node_kind_to_string(const Win95ProbeNodeKind kind) {
  switch (kind) {
    case Win95ProbeNodeKind::Missing:
      return "missing";
    case Win95ProbeNodeKind::File:
      return "file";
    case Win95ProbeNodeKind::Directory:
      return "directory";
    case Win95ProbeNodeKind::Other:
      return "other";
  }

  return "other";
}

[[nodiscard]] std::string file_kind_to_string(const Win95ProbeFileKind kind) {
  switch (kind) {
    case Win95ProbeFileKind::TextLike:
      return "text-like";
    case Win95ProbeFileKind::OpaqueBinary:
      return "opaque-binary";
    case Win95ProbeFileKind::PossibleContainer:
      return "possible-container";
    case Win95ProbeFileKind::ExecutableLike:
      return "executable-like";
  }

  return "opaque-binary";
}

[[nodiscard]] std::optional<Win95ProbeFileDetails> probe_file_details(const std::filesystem::path& path,
                                                                      const std::filesystem::path& relative_path,
                                                                      const std::size_t max_file_load_bytes,
                                                                      Win95DataProbeError& error) {
  const auto loaded = load_file_to_memory(path, max_file_load_bytes);
  if (!loaded.ok()) {
    error.path = relative_path;
    error.message = loaded.error->message;
    return std::nullopt;
  }

  const auto& file = loaded.value.value();
  const auto probe = probe_loaded_binary(file);

  Win95ProbeFileDetails details;
  details.size_bytes = file.bytes.size();

  const auto signature_size = std::min(k_signature_preview_bytes, file.bytes.size());
  const auto signature_span = std::span<const std::uint8_t>(file.bytes.data(), signature_size);
  details.signature_hex = to_hex_signature(signature_span);
  details.signature_ascii = to_ascii_signature(signature_span);
  details.first_u16_le = probe.first_u16_le;
  details.first_u32_le = probe.first_u32_le;
  details.kind = classify_file_kind(relative_path, probe, std::span<const std::uint8_t>(file.bytes.data(), file.bytes.size()));
  return details;
}

[[nodiscard]] std::optional<Win95ProbeDirectoryDetails> probe_directory_details(const std::filesystem::path& path,
                                                                                 const std::filesystem::path& relative_path,
                                                                                 const std::size_t max_file_load_bytes,
                                                                                 Win95DataProbeError& error) {
  std::vector<std::filesystem::directory_entry> entries;
  try {
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
      entries.push_back(entry);
      if (entries.size() >= k_directory_scan_entry_limit) {
        break;
      }
    }
  } catch (const std::filesystem::filesystem_error& ex) {
    error.path = relative_path;
    error.message = ex.what();
    return std::nullopt;
  }

  std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
    return left.path().filename().generic_string() < right.path().filename().generic_string();
  });

  Win95ProbeDirectoryDetails details;
  details.scanned_entry_count = entries.size();

  try {
    std::size_t actual_count = 0;
    for (const auto& ignored : std::filesystem::directory_iterator(path)) {
      (void)ignored;
      ++actual_count;
      if (actual_count > k_directory_scan_entry_limit) {
        details.truncated = true;
        break;
      }
    }
  } catch (const std::filesystem::filesystem_error& ex) {
    error.path = relative_path;
    error.message = ex.what();
    return std::nullopt;
  }

  const auto preview_count = std::min(k_directory_preview_entries, entries.size());
  details.preview_entries.reserve(preview_count);

  for (std::size_t index = 0; index < preview_count; ++index) {
    const auto& entry = entries[index];
    Win95DirectoryEntryHint hint;
    hint.name = entry.path().filename().generic_string();

    std::error_code ec;
    const bool is_file = entry.is_regular_file(ec);
    if (ec) {
      hint.kind = Win95ProbeNodeKind::Other;
      details.preview_entries.push_back(std::move(hint));
      continue;
    }

    if (is_file) {
      hint.kind = Win95ProbeNodeKind::File;
      const auto relative_entry_path = relative_path / entry.path().filename();
      Win95DataProbeError file_error;
      const auto file_details = probe_file_details(entry.path(), relative_entry_path, max_file_load_bytes, file_error);
      if (file_details.has_value()) {
        hint.size_bytes = file_details->size_bytes;
        hint.signature_hex = file_details->signature_hex.empty() ? std::optional<std::string>("(empty)")
                                                                 : file_details->signature_hex;
        hint.file_kind = file_details->kind;
      }
      details.preview_entries.push_back(std::move(hint));
      continue;
    }

    if (entry.is_directory(ec) && !ec) {
      hint.kind = Win95ProbeNodeKind::Directory;
    } else {
      hint.kind = Win95ProbeNodeKind::Other;
    }

    details.preview_entries.push_back(std::move(hint));
  }

  return details;
}

[[nodiscard]] std::optional<Win95InstallEntryReport> probe_entry(const std::filesystem::path& data_root,
                                                                  const std::string_view entry_name,
                                                                  const std::size_t max_file_load_bytes,
                                                                  Win95DataProbeError& error) {
  Win95InstallEntryReport report;
  report.entry_name = std::string(entry_name);
  report.relative_path = std::filesystem::path(entry_name);

  const auto path = data_root / report.relative_path;
  std::error_code ec;
  const bool exists = std::filesystem::exists(path, ec);
  if (ec) {
    error.path = report.relative_path;
    error.message = ec.message();
    return std::nullopt;
  }

  if (!exists) {
    report.node_kind = Win95ProbeNodeKind::Missing;
    return report;
  }

  if (std::filesystem::is_regular_file(path, ec) && !ec) {
    report.node_kind = Win95ProbeNodeKind::File;
    const auto details = probe_file_details(path, report.relative_path, max_file_load_bytes, error);
    if (!details.has_value()) {
      return std::nullopt;
    }

    report.file = details;
    return report;
  }

  if (std::filesystem::is_directory(path, ec) && !ec) {
    report.node_kind = Win95ProbeNodeKind::Directory;
    const auto details = probe_directory_details(path, report.relative_path, max_file_load_bytes, error);
    if (!details.has_value()) {
      return std::nullopt;
    }

    report.directory = details;
    return report;
  }

  report.node_kind = Win95ProbeNodeKind::Other;
  return report;
}

[[nodiscard]] Win95DataPairSummary summarize_pair(const Win95InstallEntryReport& data_entry,
                                                  const Win95InstallEntryReport& data0_entry) {
  Win95DataPairSummary summary;

  if (data_entry.node_kind == Win95ProbeNodeKind::Missing || data0_entry.node_kind == Win95ProbeNodeKind::Missing) {
    summary.relationship = "incomplete";
    summary.hint = "At least one target is missing; paired container inference is unavailable.";
    return summary;
  }

  if (data_entry.node_kind == data0_entry.node_kind) {
    summary.relationship = "same-kind";
  } else {
    summary.relationship = "different-kind";
    summary.hint = "DATA and DATA0 differ by node kind; unlikely to be a simple mirrored pair.";
    return summary;
  }

  if (data_entry.node_kind == Win95ProbeNodeKind::File && data_entry.file.has_value() && data0_entry.file.has_value()) {
    const auto data_size = static_cast<long long>(data_entry.file->size_bytes);
    const auto data0_size = static_cast<long long>(data0_entry.file->size_bytes);
    summary.size_delta_bytes = data0_size - data_size;

    if (data_entry.file->kind == Win95ProbeFileKind::PossibleContainer &&
        data0_entry.file->kind == Win95ProbeFileKind::PossibleContainer) {
      summary.hint = "Both entries look container-like and may form a paired index/data bundle.";
      return summary;
    }

    summary.hint = "Both entries are files but remain reconnaissance-only; container relationship is unknown.";
    return summary;
  }

  if (data_entry.node_kind == Win95ProbeNodeKind::Directory && data_entry.directory.has_value() &&
      data0_entry.directory.has_value()) {
    const auto data_count = static_cast<long long>(data_entry.directory->scanned_entry_count);
    const auto data0_count = static_cast<long long>(data0_entry.directory->scanned_entry_count);
    summary.size_delta_bytes = data0_count - data_count;
    summary.hint = "Both entries are directories; compare preview listings to prioritize decoder targets.";
    return summary;
  }

  summary.hint = "Both entries share the same node kind, but detailed pair inference is unknown.";
  return summary;
}

}  // namespace

ProbeWin95DataResult probe_win95_data_entries(const std::filesystem::path& data_root,
                                              const std::size_t max_file_load_bytes) {
  Win95DataProbeError error;
  const auto data_entry = probe_entry(data_root, "DATA", max_file_load_bytes, error);
  if (!data_entry.has_value()) {
    return {.error = error};
  }

  const auto data0_entry = probe_entry(data_root, "DATA0", max_file_load_bytes, error);
  if (!data0_entry.has_value()) {
    return {.error = error};
  }

  Win95DataProbeReport report;
  report.data_root = data_root;
  report.data_entry = data_entry.value();
  report.data0_entry = data0_entry.value();
  report.pair_summary = summarize_pair(report.data_entry, report.data0_entry);
  return {.value = std::move(report)};
}

std::string format_win95_data_probe_report(const Win95DataProbeReport& report) {
  std::ostringstream output;
  output << "# Caesar II Win95 DATA/DATA0 Probe\n";
  output << "data_root: " << report.data_root.generic_string() << "\n";

  const auto format_scalar = [](const std::optional<std::uint32_t>& value) -> std::string {
    if (!value.has_value()) {
      return "unavailable";
    }

    return std::to_string(value.value());
  };

  const auto format_u16 = [](const std::optional<std::uint16_t>& value) -> std::string {
    if (!value.has_value()) {
      return "unavailable";
    }

    return std::to_string(value.value());
  };

  const auto format_entry = [&](const Win95InstallEntryReport& entry) {
    output << "\n[entry]\n";
    output << "name: " << entry.entry_name << "\n";
    output << "path: " << entry.relative_path.generic_string() << "\n";
    output << "node_kind: " << node_kind_to_string(entry.node_kind) << "\n";

    if (entry.file.has_value()) {
      output << "size_bytes: " << entry.file->size_bytes << "\n";
      output << "signature_hex: " << (entry.file->signature_hex.empty() ? "(empty)" : entry.file->signature_hex) << "\n";
      output << "signature_ascii: " << (entry.file->signature_ascii.empty() ? "(empty)" : entry.file->signature_ascii)
             << "\n";
      output << "first_u16_le: " << format_u16(entry.file->first_u16_le) << "\n";
      output << "first_u32_le: " << format_scalar(entry.file->first_u32_le) << "\n";
      output << "kind: " << file_kind_to_string(entry.file->kind) << "\n";
    }

    if (entry.directory.has_value()) {
      output << "directory_scanned_entry_count: " << entry.directory->scanned_entry_count << "\n";
      output << "directory_truncated: " << (entry.directory->truncated ? "yes" : "no") << "\n";
      output << "directory_preview_entries:\n";
      for (const auto& preview_entry : entry.directory->preview_entries) {
        output << "  - name: " << preview_entry.name << "\n";
        output << "    node_kind: " << node_kind_to_string(preview_entry.kind) << "\n";
        if (preview_entry.size_bytes.has_value()) {
          output << "    size_bytes: " << preview_entry.size_bytes.value() << "\n";
        }

        if (preview_entry.signature_hex.has_value()) {
          output << "    signature_hex: " << preview_entry.signature_hex.value() << "\n";
        }

        if (preview_entry.file_kind.has_value()) {
          output << "    kind: " << file_kind_to_string(preview_entry.file_kind.value()) << "\n";
        }
      }
    }
  };

  format_entry(report.data_entry);
  format_entry(report.data0_entry);

  output << "\n[pair]\n";
  output << "relationship: " << report.pair_summary.relationship << "\n";
  if (report.pair_summary.size_delta_bytes.has_value()) {
    output << "size_delta: " << report.pair_summary.size_delta_bytes.value() << "\n";
  } else {
    output << "size_delta: unavailable\n";
  }

  output << "hint: " << report.pair_summary.hint << "\n";
  return output.str();
}

}  // namespace romulus::data
