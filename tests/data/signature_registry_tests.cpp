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
  candidate.size_bytes = 1024;
  if (!preview_text.empty()) {
    candidate.text_preview = romulus::data::CandidateTextPreview{
        .bytes_previewed = preview_text.size(),
        .truncated = false,
        .text = std::move(preview_text),
    };
  }

  return candidate;
}

int test_mz_executable_ranks_first_high() {
  const auto candidate = make_candidate("CAESAR.EXE", romulus::data::CandidateFileKind::MzExecutableLike, "MZ....");
  romulus::data::CandidateProbeBundleReport probe;

  const auto result = romulus::data::match_candidate_signatures(probe, candidate);
  if (assert_true(!result.matches.empty(), "MZ candidate should produce matches") != 0) {
    return 1;
  }

  if (assert_true(result.matches.front().decoder_id == romulus::data::CandidateDecoderId::DosExecutable,
                  "DosExecutable should rank first for MZ candidate") != 0) {
    return 1;
  }

  if (assert_true(result.matches.front().confidence == romulus::data::SignatureConfidence::High,
                  "DosExecutable should map to high confidence") != 0) {
    return 1;
  }

  return 0;
}

int test_ini_like_ranks_above_plain_text() {
  const auto candidate = make_candidate("RESOURCE.INI",
                                        romulus::data::CandidateFileKind::TextLike,
                                        "[sett....",
                                        "[sound]\nvolume=75\n");
  romulus::data::CandidateProbeBundleReport probe;

  const auto result = romulus::data::match_candidate_signatures(probe, candidate);
  if (assert_true(result.matches.size() >= 2, "INI-like candidate should produce multiple matches") != 0) {
    return 1;
  }

  if (assert_true(result.matches[0].decoder_id == romulus::data::CandidateDecoderId::IniLikeText,
                  "IniLikeText should rank above PlainText") != 0) {
    return 1;
  }

  if (assert_true(result.matches[1].decoder_id == romulus::data::CandidateDecoderId::PlainText,
                  "PlainText should be second for INI-like preview") != 0) {
    return 1;
  }

  return 0;
}

int test_plain_text_fallback_ranks_first() {
  const auto candidate = make_candidate("README.TXT",
                                        romulus::data::CandidateFileKind::TextLike,
                                        "Plaintext",
                                        "this is a generic text payload\nwithout ini markers\n");
  romulus::data::CandidateProbeBundleReport probe;

  const auto result = romulus::data::match_candidate_signatures(probe, candidate);
  if (assert_true(!result.matches.empty(), "Plain text candidate should produce matches") != 0) {
    return 1;
  }

  if (assert_true(result.matches.front().decoder_id == romulus::data::CandidateDecoderId::PlainText,
                  "PlainText should rank first for generic text") != 0) {
    return 1;
  }

  return 0;
}

int test_dat_pairing_boost_ranks_dat_decoder_first() {
  const auto candidate = make_candidate("RESOURCE.DAT", romulus::data::CandidateFileKind::OpaqueBinary, "....");
  romulus::data::CandidateProbeBundleReport probe;
  probe.dat_ix_pairs.push_back({
      .dat_relative_path = "RESOURCE.DAT",
      .ix_relative_path = "RESOURCE.IX",
      .dat_size_bytes = 100,
      .ix_size_bytes = 100,
  });

  const auto result = romulus::data::match_candidate_signatures(probe, candidate);
  if (assert_true(!result.matches.empty(), "DAT pairing candidate should produce matches") != 0) {
    return 1;
  }

  if (assert_true(result.matches.front().decoder_id == romulus::data::CandidateDecoderId::Caesar2DatContainer,
                  "Caesar2DatContainer should rank first for paired DAT") != 0) {
    return 1;
  }

  return 0;
}

int test_ix_pairing_boost_ranks_ix_decoder_first() {
  const auto candidate = make_candidate("RESOURCE.IX", romulus::data::CandidateFileKind::OpaqueBinary, "....");
  romulus::data::CandidateProbeBundleReport probe;
  probe.dat_ix_pairs.push_back({
      .dat_relative_path = "RESOURCE.DAT",
      .ix_relative_path = "RESOURCE.IX",
      .dat_size_bytes = 100,
      .ix_size_bytes = 100,
  });

  const auto result = romulus::data::match_candidate_signatures(probe, candidate);
  if (assert_true(!result.matches.empty(), "IX pairing candidate should produce matches") != 0) {
    return 1;
  }

  if (assert_true(result.matches.front().decoder_id == romulus::data::CandidateDecoderId::Caesar2IxIndex,
                  "Caesar2IxIndex should rank first for paired IX") != 0) {
    return 1;
  }

  return 0;
}

int test_unknown_binary_fallback_ranks_first() {
  const auto candidate = make_candidate("RESOURCE.BIN", romulus::data::CandidateFileKind::OpaqueBinary, "....");
  romulus::data::CandidateProbeBundleReport probe;

  const auto result = romulus::data::match_candidate_signatures(probe, candidate);
  if (assert_true(!result.matches.empty(), "Opaque binary should produce matches") != 0) {
    return 1;
  }

  if (assert_true(result.matches.front().decoder_id == romulus::data::CandidateDecoderId::UnknownBinary,
                  "UnknownBinary should rank first for opaque binary fallback") != 0) {
    return 1;
  }

  return 0;
}

int test_deterministic_ordering_for_ties() {
  const auto candidate = make_candidate("RESOURCE.DAT", romulus::data::CandidateFileKind::OpaqueBinary, "....");
  romulus::data::CandidateProbeBundleReport probe;

  const auto result = romulus::data::match_candidate_signatures(probe, candidate);
  if (assert_true(result.matches.size() >= 2, "Tie scenario should include multiple matches") != 0) {
    return 1;
  }

  const auto first_decoder = result.matches[0].decoder_id;
  const auto second_decoder = result.matches[1].decoder_id;
  if (assert_true(first_decoder == romulus::data::CandidateDecoderId::Caesar2DatContainer,
                  "Expected Caesar2DatContainer to win deterministic tie") != 0) {
    return 1;
  }

  if (assert_true(second_decoder == romulus::data::CandidateDecoderId::Caesar2IxIndex,
                  "Expected Caesar2IxIndex to follow deterministic tie-break") != 0) {
    return 1;
  }

  return 0;
}

int test_report_includes_probe_derived_fields_only() {
  const auto candidate = make_candidate("RESOURCE.CFG",
                                        romulus::data::CandidateFileKind::TextLike,
                                        "[data]",
                                        "[core]\nvalue=1\n");
  romulus::data::CandidateProbeBundleReport probe;

  const auto result = romulus::data::match_candidate_signatures(probe, candidate);
  const auto text = romulus::data::format_signature_registry_report(result);

  if (assert_true(text.find("Candidate: RESOURCE.CFG") != std::string::npos,
                  "formatted signature report should include candidate path") != 0) {
    return 1;
  }

  if (assert_true(text.find("Class: text-like") != std::string::npos,
                  "formatted signature report should include coarse class") != 0) {
    return 1;
  }

  return 0;
}

}  // namespace

int main() {
  std::srand(1815);

  if (test_mz_executable_ranks_first_high() != 0) {
    return EXIT_FAILURE;
  }

  if (test_ini_like_ranks_above_plain_text() != 0) {
    return EXIT_FAILURE;
  }

  if (test_plain_text_fallback_ranks_first() != 0) {
    return EXIT_FAILURE;
  }

  if (test_dat_pairing_boost_ranks_dat_decoder_first() != 0) {
    return EXIT_FAILURE;
  }

  if (test_ix_pairing_boost_ranks_ix_decoder_first() != 0) {
    return EXIT_FAILURE;
  }

  if (test_unknown_binary_fallback_ranks_first() != 0) {
    return EXIT_FAILURE;
  }

  if (test_deterministic_ordering_for_ties() != 0) {
    return EXIT_FAILURE;
  }

  if (test_report_includes_probe_derived_fields_only() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
