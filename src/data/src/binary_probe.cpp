#include "romulus/data/binary_probe.h"

#include <iomanip>
#include <sstream>

namespace romulus::data {
namespace {

[[nodiscard]] std::string to_hex_signature(std::span<const std::byte> bytes) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    if (index != 0) {
      stream << ' ';
    }

    stream << std::setw(2) << static_cast<int>(std::to_integer<std::uint8_t>(bytes[index]));
  }

  return stream.str();
}

[[nodiscard]] std::optional<DosMzHeaderSummary> try_parse_dos_mz_header(const LoadedFile& file) {
  BinaryReader reader = make_binary_reader(file);

  const auto signature = reader.read_u16_le();
  if (!signature.ok() || signature.value.value() != 0x5A4D) {
    return std::nullopt;
  }

  if (file.bytes.size() < 64) {
    return std::nullopt;
  }

  if (reader.seek(2).has_value()) {
    return std::nullopt;
  }

  const auto bytes_in_last_page = reader.read_u16_le();
  const auto pages_in_file = reader.read_u16_le();
  if (!bytes_in_last_page.ok() || !pages_in_file.ok()) {
    return std::nullopt;
  }

  if (reader.seek(24).has_value()) {
    return std::nullopt;
  }

  const auto relocation_table_offset = reader.read_u16_le();
  if (!relocation_table_offset.ok()) {
    return std::nullopt;
  }

  if (reader.seek(60).has_value()) {
    return std::nullopt;
  }

  const auto pe_header_offset = reader.read_u32_le();
  if (!pe_header_offset.ok()) {
    return std::nullopt;
  }

  return DosMzHeaderSummary{
      .bytes_in_last_page = bytes_in_last_page.value.value(),
      .pages_in_file = pages_in_file.value.value(),
      .relocation_table_offset = relocation_table_offset.value.value(),
      .pe_header_offset = pe_header_offset.value.value(),
  };
}

}  // namespace

BinaryProbeReport probe_loaded_binary(const LoadedFile& file) {
  BinaryProbeReport report;
  report.source = file.path.string();
  report.size_bytes = file.bytes.size();

  BinaryReader signature_reader = make_binary_reader(file);
  const auto signature_length = file.bytes.size() < 8 ? file.bytes.size() : 8;
  const auto signature = signature_reader.read_bytes(signature_length);
  if (signature.ok()) {
    report.signature_hex = to_hex_signature(signature.value.value());
  }

  BinaryReader scalar_reader = make_binary_reader(file);
  const auto first_u16 = scalar_reader.read_u16_le();
  if (first_u16.ok()) {
    report.first_u16_le = first_u16.value.value();
  }

  if (scalar_reader.seek(0).has_value()) {
    return report;
  }

  const auto first_u32 = scalar_reader.read_u32_le();
  if (first_u32.ok()) {
    report.first_u32_le = first_u32.value.value();
  }

  report.dos_mz_header = try_parse_dos_mz_header(file);
  return report;
}

std::string format_binary_probe_report(const BinaryProbeReport& report) {
  std::ostringstream output;
  output << "# Caesar II Binary Probe\n";
  output << "source: " << report.source << "\n";
  output << "size_bytes: " << report.size_bytes << "\n";
  output << "signature_hex: " << (report.signature_hex.empty() ? "(empty)" : report.signature_hex) << "\n";

  if (report.first_u16_le.has_value()) {
    output << "first_u16_le: " << report.first_u16_le.value() << "\n";
  } else {
    output << "first_u16_le: unavailable\n";
  }

  if (report.first_u32_le.has_value()) {
    output << "first_u32_le: " << report.first_u32_le.value() << "\n";
  } else {
    output << "first_u32_le: unavailable\n";
  }

  if (report.dos_mz_header.has_value()) {
    const auto& header = report.dos_mz_header.value();
    output << "dos_mz_header: present\n";
    output << "  bytes_in_last_page: " << header.bytes_in_last_page << "\n";
    output << "  pages_in_file: " << header.pages_in_file << "\n";
    output << "  relocation_table_offset: " << header.relocation_table_offset << "\n";
    output << "  pe_header_offset: " << header.pe_header_offset << "\n";
  } else {
    output << "dos_mz_header: absent\n";
  }

  return output.str();
}

}  // namespace romulus::data
