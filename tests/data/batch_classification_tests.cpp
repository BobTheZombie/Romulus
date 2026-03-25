#include "romulus/data/signature_registry.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

int assert_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    return 1;
  }

  return 0;
}

romulus::data::CandidateFileReport make_candidate(std::string path,
                                                  romulus::data::CandidateFileKind kind,
                                                  std::string signature_ascii,
                                                  std::string preview_text = {}) {
  romulus::data::CandidateFileReport candidate;
  candidate.relative_path = std::filesystem::path(path);
  candidate.kind = kind;
  candidate.signature_ascii = std::move(signature_ascii);
  candidate.size_bytes = 256;
  if (!preview_text.empty()) {
    candidate.text_preview = romulus::data::CandidateTextPreview{
        .bytes_previewed = preview_text.size(),
        .truncated = false,
        .text = std::move(preview_text),
    };
  }

  return candidate;
}

int test_batch_classification_is_deterministically_sorted() {
  romulus::data::CandidateProbeBundleReport probe;
  probe.files.push_back(
      make_candidate("ZZZ.BIN", romulus::data::CandidateFileKind::OpaqueBinary, "...."));
  probe.files.push_back(
      make_candidate("AUDIO.CFG", romulus::data::CandidateFileKind::TextLike, "[aud]", "[audio]\nvolume=1\n"));
  probe.files.push_back(
      make_candidate("BINARY.DAT", romulus::data::CandidateFileKind::OpaqueBinary, "...."));
  probe.files.push_back(
      make_candidate("BINARY.IX", romulus::data::CandidateFileKind::OpaqueBinary, "...."));

  probe.dat_ix_pairs.push_back({
      .dat_relative_path = "BINARY.DAT",
      .ix_relative_path = "BINARY.IX",
      .dat_size_bytes = 10,
      .ix_size_bytes = 10,
  });

  const auto report = romulus::data::classify_candidate_batch(probe, false);
  if (assert_true(report.files.size() == 4, "expected one classification summary per file") != 0) {
    return 1;
  }

  if (assert_true(report.files[0].path.generic_string() == "AUDIO.CFG", "expected sorted path order") != 0) {
    return 1;
  }

  if (assert_true(report.files[1].path.generic_string() == "BINARY.DAT", "expected sorted path order") != 0) {
    return 1;
  }

  if (assert_true(report.files[2].path.generic_string() == "BINARY.IX", "expected sorted path order") != 0) {
    return 1;
  }

  if (assert_true(report.files[3].path.generic_string() == "ZZZ.BIN", "expected sorted path order") != 0) {
    return 1;
  }

  if (assert_true(report.files[3].top_decoder_id == romulus::data::CandidateDecoderId::UnknownBinary,
                  "opaque binary should prefer unknown fallback") != 0) {
    return 1;
  }

  return 0;
}

int test_batch_report_format_and_secondary_output() {
  romulus::data::CandidateProbeBundleReport probe;
  probe.files.push_back(
      make_candidate("RESOURCE.INI", romulus::data::CandidateFileKind::TextLike, "[core]", "[core]\nvalue=1\n"));

  const auto report = romulus::data::classify_candidate_batch(probe, true);
  const auto text = romulus::data::format_batch_classification_report(report);

  if (assert_true(text.find("# Caesar II Batch Candidate Classification") != std::string::npos,
                  "report should include a stable header") != 0) {
    return 1;
  }

  if (assert_true(text.find("- file: RESOURCE.INI") != std::string::npos,
                  "report should include file name") != 0) {
    return 1;
  }

  if (assert_true(text.find("top_decoder: IniLikeText") != std::string::npos,
                  "report should include top-ranked decoder") != 0) {
    return 1;
  }

  if (assert_true(text.find("confidence: high") != std::string::npos,
                  "report should include confidence bucket") != 0) {
    return 1;
  }

  if (assert_true(text.find("evidence: ") != std::string::npos,
                  "report should include short evidence summary") != 0) {
    return 1;
  }

  if (assert_true(text.find("secondary: ") != std::string::npos,
                  "secondary match line should be emitted when requested") != 0) {
    return 1;
  }

  return 0;
}

}  // namespace

int main() {
  std::srand(1815);

  if (test_batch_classification_is_deterministically_sorted() != 0) {
    return EXIT_FAILURE;
  }

  if (test_batch_report_format_and_secondary_output() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
