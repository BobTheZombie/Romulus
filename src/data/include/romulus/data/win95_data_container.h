#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "romulus/data/binary_reader.h"
#include "romulus/data/ilbm_image.h"

namespace romulus::data {

struct Win95PackContainerHeader {
  static constexpr std::size_t kHeaderSize = 8;
  static constexpr std::size_t kEntrySize = 8;

  std::string signature;
  std::uint32_t entry_count = 0;
  std::size_t entry_table_offset = kHeaderSize;
  std::size_t entry_table_size = 0;
};

struct Win95PackContainerEntry {
  std::size_t index = 0;
  std::size_t offset = 0;
  std::size_t size = 0;
  std::size_t end_offset = 0;
  std::string signature_hex;
  std::string signature_ascii;
  std::string classification_hint;
  bool has_recognizable_signature = false;
};

struct Win95PackEntrySizeBuckets {
  std::size_t tiny_bytes_0_to_255 = 0;
  std::size_t small_bytes_256_to_4095 = 0;
  std::size_t medium_bytes_4096_to_65535 = 0;
  std::size_t large_bytes_65536_plus = 0;
};

struct Win95PackMagicFrequency {
  std::string signature;
  std::size_t count = 0;
};

struct Win95PackContainerSummary {
  std::size_t entry_count = 0;
  std::size_t total_payload_bytes = 0;
  std::size_t recognizable_signature_count = 0;
  Win95PackEntrySizeBuckets size_buckets;
  std::vector<Win95PackMagicFrequency> magic_frequencies;
};

struct Win95PackContainerResource {
  Win95PackContainerHeader header;
  std::vector<Win95PackContainerEntry> entries;
  Win95PackContainerSummary summary;
};

struct Win95PackIlbmExtraction {
  Win95PackContainerEntry entry;
  std::vector<std::uint8_t> payload_bytes;
  IlbmImageResource ilbm;
};

struct Win95PackTextExtraction {
  Win95PackContainerEntry entry;
  std::vector<std::uint8_t> payload_bytes;
  std::string decoded_text;
  std::size_t line_count = 0;
  std::size_t character_count = 0;
};

struct Win95PackIlbmBatchEntryResult {
  std::size_t entry_index = 0;
  std::size_t offset = 0;
  std::size_t size = 0;
  std::string classification_hint;
  bool parse_success = false;
  std::optional<std::string> failure_reason;
  std::optional<std::size_t> width;
  std::optional<std::size_t> height;
  std::optional<std::size_t> palette_color_count;
};

struct Win95PackIlbmBatchResult {
  std::size_t total_entry_count = 0;
  std::size_t candidate_entry_count = 0;
  std::size_t parsed_entry_count = 0;
  std::size_t failed_entry_count = 0;
  std::vector<Win95PackIlbmBatchEntryResult> entry_results;
  std::vector<Win95PackMagicFrequency> failure_reason_frequencies;
};

struct Win95PackReportOptions {
  std::size_t preview_entry_limit = 8;
  bool include_all_entries = false;
};

struct Win95PackIlbmBatchReportOptions {
  std::size_t preview_entry_limit = 8;
  bool include_all_entries = false;
};

struct Win95PackIlbmIndexEntry {
  std::size_t entry_index = 0;
  std::size_t offset = 0;
  std::size_t size = 0;
  std::size_t width = 0;
  std::size_t height = 0;
  std::optional<std::size_t> palette_color_count;
  std::string classification_hint;
};

struct Win95PackIlbmIndex {
  std::size_t total_entry_count = 0;
  std::size_t candidate_entry_count = 0;
  std::size_t successful_entry_count = 0;
  std::vector<Win95PackIlbmIndexEntry> successful_entries;
};

struct Win95PackIlbmExportResult {
  std::size_t requested_entry_index = 0;
  bool success = false;
  std::optional<std::filesystem::path> output_path;
  std::optional<std::string> failure_reason;
};

struct Win95PackIlbmIndexReportOptions {
  std::size_t preview_entry_limit = 8;
  bool include_all_entries = false;
};

struct Win95PackTextReportOptions {
  std::size_t preview_character_limit = 160;
};

struct Win95DataContainerProbeError {
  std::filesystem::path requested_path;
  std::string message;
};

struct ProbeWin95DataContainerResult {
  std::optional<Win95PackContainerResource> value;
  std::optional<Win95DataContainerProbeError> error;

  [[nodiscard]] bool ok() const {
    return value.has_value();
  }
};

constexpr std::size_t k_default_win95_container_max_file_load_bytes = 64 * 1024 * 1024;

[[nodiscard]] ParseResult<Win95PackContainerResource> parse_win95_pack_container(std::span<const std::byte> bytes);
[[nodiscard]] ParseResult<Win95PackContainerResource> parse_win95_pack_container(std::span<const std::uint8_t> bytes);

[[nodiscard]] ParseResult<Win95PackIlbmExtraction> extract_win95_pack_ilbm_entry(
    std::span<const std::byte> container_bytes,
    const Win95PackContainerResource& container,
    std::size_t entry_index);
[[nodiscard]] ParseResult<Win95PackIlbmExtraction> extract_win95_pack_ilbm_entry(
    std::span<const std::uint8_t> container_bytes,
    const Win95PackContainerResource& container,
    std::size_t entry_index);
[[nodiscard]] ParseResult<Win95PackTextExtraction> extract_win95_pack_text_entry(
    std::span<const std::byte> container_bytes,
    const Win95PackContainerResource& container,
    std::size_t entry_index);
[[nodiscard]] ParseResult<Win95PackTextExtraction> extract_win95_pack_text_entry(
    std::span<const std::uint8_t> container_bytes,
    const Win95PackContainerResource& container,
    std::size_t entry_index);

[[nodiscard]] Win95PackIlbmBatchResult analyze_win95_pack_ilbm_batch(std::span<const std::byte> container_bytes,
                                                                      const Win95PackContainerResource& container);
[[nodiscard]] Win95PackIlbmBatchResult analyze_win95_pack_ilbm_batch(
    std::span<const std::uint8_t> container_bytes,
    const Win95PackContainerResource& container);
[[nodiscard]] Win95PackIlbmIndex build_win95_pack_ilbm_success_index(const Win95PackIlbmBatchResult& batch_result);
[[nodiscard]] std::optional<Win95PackIlbmIndexEntry> find_win95_pack_ilbm_index_entry(
    const Win95PackIlbmIndex& index,
    std::size_t entry_index);

[[nodiscard]] ProbeWin95DataContainerResult probe_win95_data_container_file(
    const std::filesystem::path& data_root,
    const std::string& candidate_path,
    std::size_t max_file_load_bytes = k_default_win95_container_max_file_load_bytes);

[[nodiscard]] std::string format_win95_data_container_report(const Win95PackContainerResource& resource,
                                                             std::string_view source_label = "",
                                                             Win95PackReportOptions options = {});
[[nodiscard]] std::string format_win95_pack_ilbm_batch_report(
    const Win95PackIlbmBatchResult& result,
    std::string_view source_label = "",
    Win95PackIlbmBatchReportOptions options = {});
[[nodiscard]] std::string format_win95_pack_ilbm_index_report(
    const Win95PackIlbmIndex& index,
    std::string_view source_label = "",
    Win95PackIlbmIndexReportOptions options = {});
[[nodiscard]] std::string format_win95_pack_text_report(const Win95PackTextExtraction& extraction,
                                                        std::string_view source_label = "",
                                                        Win95PackTextReportOptions options = {});

}  // namespace romulus::data
