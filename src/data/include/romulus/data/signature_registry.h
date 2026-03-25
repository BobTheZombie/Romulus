#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "romulus/data/candidate_probe.h"

namespace romulus::data {

enum class CandidateDecoderId {
  None,
  PlainText,
  IniLikeText,
  DosExecutable,
  Caesar2DatContainer,
  Caesar2IxIndex,
  UnknownBinary,
};

enum class SignatureEvidenceKind {
  ExtensionHint,
  MagicBytes,
  StructuralHint,
  PairingHint,
  TextPatternHint,
  FallbackHint,
};

enum class SignatureConfidence {
  Low,
  Medium,
  High,
};

struct SignatureEvidence {
  SignatureEvidenceKind kind = SignatureEvidenceKind::FallbackHint;
  std::string detail;
  int weight = 0;
};

struct SignatureMatch {
  CandidateDecoderId decoder_id = CandidateDecoderId::None;
  int score = 0;
  SignatureConfidence confidence = SignatureConfidence::Low;
  std::vector<SignatureEvidence> evidence;
};

struct SignatureRegistryResult {
  std::filesystem::path path;
  std::size_t size_bytes = 0;
  CandidateFileKind kind = CandidateFileKind::OpaqueBinary;
  bool has_matching_dat_pair = false;
  bool has_matching_ix_pair = false;
  std::vector<SignatureMatch> matches;
};

struct BatchClassificationSummary {
  std::filesystem::path path;
  CandidateDecoderId top_decoder_id = CandidateDecoderId::None;
  SignatureConfidence confidence = SignatureConfidence::Low;
  std::string evidence_summary;
  std::vector<SignatureMatch> secondary_matches;
};

struct BatchClassificationReport {
  std::vector<BatchClassificationSummary> files;
};

[[nodiscard]] SignatureRegistryResult match_candidate_signatures(const CandidateProbeBundleReport& probe_bundle,
                                                                 const CandidateFileReport& candidate);
[[nodiscard]] std::string format_signature_registry_report(const SignatureRegistryResult& result);

[[nodiscard]] BatchClassificationReport classify_candidate_batch(const CandidateProbeBundleReport& probe_bundle,
                                                                bool include_secondary_matches = false);
[[nodiscard]] std::string format_batch_classification_report(const BatchClassificationReport& report);

[[nodiscard]] std::string to_string(CandidateDecoderId decoder_id);
[[nodiscard]] std::string to_string(SignatureEvidenceKind kind);
[[nodiscard]] std::string to_string(SignatureConfidence confidence);
[[nodiscard]] std::string to_string(CandidateFileKind kind);

}  // namespace romulus::data
