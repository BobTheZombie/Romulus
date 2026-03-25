#include "romulus/data/signature_registry.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace romulus::data {
namespace {

[[nodiscard]] std::string to_upper_ascii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
    return static_cast<char>(std::toupper(character));
  });
  return value;
}

[[nodiscard]] std::string normalize_key(const std::filesystem::path& value) {
  return to_upper_ascii(value.generic_string());
}

[[nodiscard]] bool has_matching_dat_pair(const CandidateProbeBundleReport& probe_bundle,
                                         const std::filesystem::path& candidate_path) {
  const auto normalized_candidate = normalize_key(candidate_path);
  return std::any_of(probe_bundle.dat_ix_pairs.begin(), probe_bundle.dat_ix_pairs.end(), [&](const auto& pair) {
    return normalize_key(pair.ix_relative_path) == normalized_candidate;
  });
}

[[nodiscard]] bool has_matching_ix_pair(const CandidateProbeBundleReport& probe_bundle,
                                        const std::filesystem::path& candidate_path) {
  const auto normalized_candidate = normalize_key(candidate_path);
  return std::any_of(probe_bundle.dat_ix_pairs.begin(), probe_bundle.dat_ix_pairs.end(), [&](const auto& pair) {
    return normalize_key(pair.dat_relative_path) == normalized_candidate;
  });
}

[[nodiscard]] SignatureConfidence map_confidence(const int score) {
  if (score >= 60) {
    return SignatureConfidence::High;
  }

  if (score >= 25) {
    return SignatureConfidence::Medium;
  }

  return SignatureConfidence::Low;
}

[[nodiscard]] int decoder_sort_rank(const CandidateDecoderId decoder_id) {
  switch (decoder_id) {
    case CandidateDecoderId::None:
      return 0;
    case CandidateDecoderId::PlainText:
      return 1;
    case CandidateDecoderId::IniLikeText:
      return 2;
    case CandidateDecoderId::DosExecutable:
      return 3;
    case CandidateDecoderId::Caesar2DatContainer:
      return 4;
    case CandidateDecoderId::Caesar2IxIndex:
      return 5;
    case CandidateDecoderId::UnknownBinary:
      return 6;
  }

  return 999;
}

[[nodiscard]] bool has_ini_section_header(const std::string& preview_text) {
  std::istringstream stream(preview_text);
  std::string line;
  while (std::getline(stream, line)) {
    const auto first = line.find_first_not_of(" \t");
    if (first == std::string::npos) {
      continue;
    }

    if (line[first] != '[') {
      continue;
    }

    const auto close = line.find(']', first + 1);
    if (close != std::string::npos && close > first + 1) {
      return true;
    }
  }

  return false;
}

[[nodiscard]] bool has_key_value_lines(const std::string& preview_text) {
  std::istringstream stream(preview_text);
  std::string line;
  while (std::getline(stream, line)) {
    const auto first = line.find_first_not_of(" \t");
    if (first == std::string::npos) {
      continue;
    }

    const auto equal = line.find('=', first);
    if (equal == std::string::npos) {
      continue;
    }

    const auto key_last = line.find_last_not_of(" \t", equal == 0 ? 0 : equal - 1);
    const auto value_first = line.find_first_not_of(" \t", equal + 1);
    if (key_last != std::string::npos && key_last >= first && value_first != std::string::npos) {
      return true;
    }
  }

  return false;
}

[[nodiscard]] bool has_non_empty_printable_preview(const CandidateFileReport& candidate) {
  if (!candidate.text_preview.has_value()) {
    return false;
  }

  for (const auto character : candidate.text_preview->text) {
    if (std::isprint(static_cast<unsigned char>(character)) != 0) {
      return true;
    }
  }

  return false;
}

void add_evidence(SignatureMatch& match, SignatureEvidenceKind kind, std::string detail, const int weight) {
  match.score += weight;
  match.evidence.push_back({
      .kind = kind,
      .detail = std::move(detail),
      .weight = weight,
  });
}

}  // namespace

SignatureRegistryResult match_candidate_signatures(const CandidateProbeBundleReport& probe_bundle,
                                                   const CandidateFileReport& candidate) {
  SignatureRegistryResult result;
  result.path = candidate.relative_path;
  result.size_bytes = candidate.size_bytes;
  result.kind = candidate.kind;
  result.has_matching_dat_pair = has_matching_dat_pair(probe_bundle, candidate.relative_path);
  result.has_matching_ix_pair = has_matching_ix_pair(probe_bundle, candidate.relative_path);

  const auto extension = to_upper_ascii(candidate.relative_path.extension().string());

  SignatureMatch dos_executable{.decoder_id = CandidateDecoderId::DosExecutable};
  if (candidate.signature_ascii.rfind("MZ", 0) == 0) {
    add_evidence(dos_executable, SignatureEvidenceKind::MagicBytes, "MZ header", 80);
  }
  if (extension == ".EXE") {
    add_evidence(dos_executable, SignatureEvidenceKind::ExtensionHint, ".exe extension", 15);
  }
  if (candidate.kind == CandidateFileKind::MzExecutableLike) {
    add_evidence(dos_executable, SignatureEvidenceKind::StructuralHint, "mz-executable-like", 10);
  }

  SignatureMatch ini_like_text{.decoder_id = CandidateDecoderId::IniLikeText};
  if (extension == ".INI") {
    add_evidence(ini_like_text, SignatureEvidenceKind::ExtensionHint, ".ini extension", 20);
  }
  if (extension == ".CFG") {
    add_evidence(ini_like_text, SignatureEvidenceKind::ExtensionHint, ".cfg extension", 15);
  }
  if (candidate.kind == CandidateFileKind::TextLike) {
    add_evidence(ini_like_text, SignatureEvidenceKind::StructuralHint, "text-like", 20);
  }
  if (candidate.text_preview.has_value() && has_ini_section_header(candidate.text_preview->text)) {
    add_evidence(ini_like_text, SignatureEvidenceKind::TextPatternHint, "contains [section] header", 20);
  }
  if (candidate.text_preview.has_value() && has_key_value_lines(candidate.text_preview->text)) {
    add_evidence(ini_like_text, SignatureEvidenceKind::TextPatternHint, "contains key=value lines", 20);
  }

  SignatureMatch plain_text{.decoder_id = CandidateDecoderId::PlainText};
  if (extension == ".TXT") {
    add_evidence(plain_text, SignatureEvidenceKind::ExtensionHint, ".txt extension", 15);
  }
  if (candidate.kind == CandidateFileKind::TextLike) {
    add_evidence(plain_text, SignatureEvidenceKind::StructuralHint, "text-like", 30);
  }
  if (has_non_empty_printable_preview(candidate)) {
    add_evidence(plain_text, SignatureEvidenceKind::TextPatternHint, "non-empty printable preview", 10);
  }

  SignatureMatch dat_container{.decoder_id = CandidateDecoderId::Caesar2DatContainer};
  if (extension == ".DAT") {
    add_evidence(dat_container, SignatureEvidenceKind::ExtensionHint, ".dat extension", 15);
  }
  if (result.has_matching_ix_pair) {
    add_evidence(dat_container, SignatureEvidenceKind::PairingHint, "matching .ix present", 35);
  }
  if (candidate.kind == CandidateFileKind::OpaqueBinary) {
    add_evidence(dat_container, SignatureEvidenceKind::StructuralHint, "opaque-binary", 10);
  }

  SignatureMatch ix_index{.decoder_id = CandidateDecoderId::Caesar2IxIndex};
  if (extension == ".IX") {
    add_evidence(ix_index, SignatureEvidenceKind::ExtensionHint, ".ix extension", 20);
  }
  if (result.has_matching_dat_pair) {
    add_evidence(ix_index, SignatureEvidenceKind::PairingHint, "matching .dat present", 35);
  }
  if (candidate.kind == CandidateFileKind::OpaqueBinary) {
    add_evidence(ix_index, SignatureEvidenceKind::StructuralHint, "opaque-binary", 10);
  }

  std::vector<SignatureMatch> matches;
  for (auto* match : {&dos_executable, &ini_like_text, &plain_text, &dat_container, &ix_index}) {
    if (match->score <= 0) {
      continue;
    }

    match->confidence = map_confidence(match->score);
    matches.push_back(*match);
  }

  SignatureMatch unknown_binary{.decoder_id = CandidateDecoderId::UnknownBinary};
  if (candidate.kind == CandidateFileKind::OpaqueBinary) {
    add_evidence(unknown_binary, SignatureEvidenceKind::StructuralHint, "opaque-binary", 10);
  }

  const auto strongest_known_it = std::max_element(matches.begin(), matches.end(), [](const auto& left, const auto& right) {
    return left.score < right.score;
  });
  const int strongest_known_score = strongest_known_it == matches.end() ? 0 : strongest_known_it->score;

  if (unknown_binary.score > 0 && strongest_known_score <= unknown_binary.score) {
    add_evidence(unknown_binary, SignatureEvidenceKind::FallbackHint, "opaque binary fallback", 10);
  }

  if (unknown_binary.score > 0) {
    unknown_binary.confidence = map_confidence(unknown_binary.score);
    matches.push_back(std::move(unknown_binary));
  }

  std::stable_sort(matches.begin(), matches.end(), [](const auto& left, const auto& right) {
    if (left.score != right.score) {
      return left.score > right.score;
    }

    return decoder_sort_rank(left.decoder_id) < decoder_sort_rank(right.decoder_id);
  });

  result.matches = std::move(matches);
  return result;
}

std::string format_signature_registry_report(const SignatureRegistryResult& result) {
  std::ostringstream output;
  output << "Candidate: " << result.path.generic_string() << "\n";
  output << "Class: " << to_string(result.kind) << "\n";
  output << "Size: " << result.size_bytes << " bytes\n";

  if (result.has_matching_ix_pair) {
    output << "Pairing: matching .ix present\n";
  } else if (result.has_matching_dat_pair) {
    output << "Pairing: matching .dat present\n";
  } else {
    output << "Pairing: none\n";
  }

  output << "\nSignature matches:\n";
  if (result.matches.empty()) {
    output << "  (none)\n";
    return output.str();
  }

  std::size_t rank = 1;
  for (const auto& match : result.matches) {
    output << "  " << rank++ << ". " << to_string(match.decoder_id) << " (score: " << match.score << ", "
           << to_string(match.confidence) << ")\n";
    for (const auto& evidence : match.evidence) {
      output << "     - " << to_string(evidence.kind) << ": " << evidence.detail << " (+" << evidence.weight << ")\n";
    }
  }

  return output.str();
}
[[nodiscard]] std::string summarize_match_evidence(const SignatureMatch& match) {
  if (match.evidence.empty()) {
    return "no evidence";
  }

  std::ostringstream output;
  const std::size_t count = std::min<std::size_t>(2, match.evidence.size());
  for (std::size_t index = 0; index < count; ++index) {
    if (index != 0) {
      output << "; ";
    }

    output << match.evidence[index].detail;
  }

  if (match.evidence.size() > count) {
    output << "; +" << (match.evidence.size() - count) << " more";
  }

  return output.str();
}

BatchClassificationReport classify_candidate_batch(const CandidateProbeBundleReport& probe_bundle,
                                                   const bool include_secondary_matches) {
  BatchClassificationReport report;
  report.files.reserve(probe_bundle.files.size());

  for (const auto& candidate : probe_bundle.files) {
    const auto matched = match_candidate_signatures(probe_bundle, candidate);

    BatchClassificationSummary summary;
    summary.path = matched.path;

    if (matched.matches.empty()) {
      summary.evidence_summary = "no signature evidence";
      report.files.push_back(std::move(summary));
      continue;
    }

    const auto& top = matched.matches.front();
    summary.top_decoder_id = top.decoder_id;
    summary.confidence = top.confidence;
    summary.evidence_summary = summarize_match_evidence(top);

    if (include_secondary_matches && matched.matches.size() > 1) {
      const auto& second = matched.matches[1];
      if (second.confidence == SignatureConfidence::High || second.confidence == SignatureConfidence::Medium) {
        summary.secondary_matches.push_back(second);
      }
    }

    report.files.push_back(std::move(summary));
  }

  std::stable_sort(report.files.begin(), report.files.end(), [](const auto& left, const auto& right) {
    return normalize_key(left.path) < normalize_key(right.path);
  });

  return report;
}

std::string format_batch_classification_report(const BatchClassificationReport& report) {
  std::ostringstream output;
  output << "# Caesar II Batch Candidate Classification\n";

  for (const auto& file : report.files) {
    output << "- file: " << file.path.generic_string() << "\n";
    output << "  top_decoder: " << to_string(file.top_decoder_id) << "\n";
    output << "  confidence: " << to_string(file.confidence) << "\n";
    output << "  evidence: " << file.evidence_summary << "\n";
    if (!file.secondary_matches.empty()) {
      const auto& secondary = file.secondary_matches.front();
      output << "  secondary: " << to_string(secondary.decoder_id) << " (" << to_string(secondary.confidence)
             << ", score " << secondary.score << ")\n";
    }
  }

  return output.str();
}
std::string to_string(const CandidateDecoderId decoder_id) {
  switch (decoder_id) {
    case CandidateDecoderId::None:
      return "None";
    case CandidateDecoderId::PlainText:
      return "PlainText";
    case CandidateDecoderId::IniLikeText:
      return "IniLikeText";
    case CandidateDecoderId::DosExecutable:
      return "DosExecutable";
    case CandidateDecoderId::Caesar2DatContainer:
      return "Caesar2DatContainer";
    case CandidateDecoderId::Caesar2IxIndex:
      return "Caesar2IxIndex";
    case CandidateDecoderId::UnknownBinary:
      return "UnknownBinary";
  }

  return "None";
}

std::string to_string(const SignatureEvidenceKind kind) {
  switch (kind) {
    case SignatureEvidenceKind::ExtensionHint:
      return "extension hint";
    case SignatureEvidenceKind::MagicBytes:
      return "magic bytes";
    case SignatureEvidenceKind::StructuralHint:
      return "structural hint";
    case SignatureEvidenceKind::PairingHint:
      return "pairing hint";
    case SignatureEvidenceKind::TextPatternHint:
      return "text pattern hint";
    case SignatureEvidenceKind::FallbackHint:
      return "fallback hint";
  }

  return "fallback hint";
}

std::string to_string(const SignatureConfidence confidence) {
  switch (confidence) {
    case SignatureConfidence::Low:
      return "low";
    case SignatureConfidence::Medium:
      return "medium";
    case SignatureConfidence::High:
      return "high";
  }

  return "low";
}

std::string to_string(const CandidateFileKind kind) {
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

}  // namespace romulus::data
