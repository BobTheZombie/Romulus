#include "romulus/data/pe_exe_resource.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>

namespace romulus::data {
namespace {

constexpr std::uint16_t kPe32OptionalMagic = 0x10B;
constexpr std::uint16_t kPe32PlusOptionalMagic = 0x20B;
constexpr std::uint32_t kOrdinalFlag32 = 0x80000000U;
constexpr std::size_t kImportDescriptorSize = 20;
constexpr std::size_t kMaxCStringLength = 512;
constexpr std::size_t kMaxImportDescriptors = 4096;
constexpr std::size_t kMaxImportsPerDescriptor = 16384;

struct SectionHeaderView {
  std::uint32_t virtual_address = 0;
  std::uint32_t virtual_size = 0;
  std::uint32_t raw_data_pointer = 0;
  std::uint32_t raw_data_size = 0;
};

[[nodiscard]] ParseError make_invalid_pe_error(std::size_t offset,
                                               std::size_t requested_bytes,
                                               std::size_t buffer_size,
                                               const std::string& message) {
  return make_invalid_format_error(offset, requested_bytes, buffer_size, message);
}

[[nodiscard]] ParseResult<std::uint16_t> read_u16_at(std::span<const std::byte> bytes, std::size_t offset) {
  BinaryReader reader(bytes);
  if (const auto seek_error = reader.seek(offset); seek_error.has_value()) {
    return {.error = seek_error};
  }
  return reader.read_u16_le();
}

[[nodiscard]] ParseResult<std::uint32_t> read_u32_at(std::span<const std::byte> bytes, std::size_t offset) {
  BinaryReader reader(bytes);
  if (const auto seek_error = reader.seek(offset); seek_error.has_value()) {
    return {.error = seek_error};
  }
  return reader.read_u32_le();
}

[[nodiscard]] ParseResult<std::string> read_c_string_at(std::span<const std::byte> bytes,
                                                        std::size_t offset,
                                                        const std::string& field_label) {
  BinaryReader reader(bytes);
  if (const auto seek_error = reader.seek(offset); seek_error.has_value()) {
    return {.error = make_invalid_pe_error(offset,
                                           1,
                                           bytes.size(),
                                           "Invalid " + field_label + " offset while decoding PE string")};
  }

  std::string value;
  value.reserve(32);

  for (std::size_t index = 0; index < kMaxCStringLength; ++index) {
    const auto byte = reader.read_u8();
    if (!byte.ok()) {
      return {.error = make_invalid_pe_error(offset,
                                             index + 1,
                                             bytes.size(),
                                             "Truncated " + field_label + " while decoding PE string")};
    }

    if (byte.value.value() == 0) {
      return {.value = value};
    }

    value.push_back(static_cast<char>(byte.value.value()));
  }

  return {.error = make_invalid_pe_error(
              offset,
              kMaxCStringLength,
              bytes.size(),
              "Unterminated or excessively long " + field_label + " (max 512 bytes)")};
}

[[nodiscard]] ParseResult<std::size_t> rva_to_offset(std::uint32_t rva,
                                                     std::span<const SectionHeaderView> sections,
                                                     std::size_t file_size) {
  for (const auto& section : sections) {
    const auto span_size = std::max(section.virtual_size, section.raw_data_size);
    if (span_size == 0) {
      continue;
    }

    const std::uint64_t section_start = section.virtual_address;
    const std::uint64_t section_end = section_start + span_size;
    if (rva < section_start || rva >= section_end) {
      continue;
    }

    const std::uint64_t delta = static_cast<std::uint64_t>(rva) - section_start;
    const std::uint64_t file_offset = static_cast<std::uint64_t>(section.raw_data_pointer) + delta;

    if (file_offset > file_size) {
      return {.error = make_invalid_pe_error(static_cast<std::size_t>(file_offset),
                                             1,
                                             file_size,
                                             "RVA maps beyond available file bytes")};
    }

    return {.value = static_cast<std::size_t>(file_offset)};
  }

  return {.error = make_invalid_pe_error(0, 0, file_size, "RVA did not map to any PE section")};
}

[[nodiscard]] std::string lower_ascii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

[[nodiscard]] bool contains_any(std::string_view haystack, std::span<const std::string_view> needles) {
  return std::any_of(needles.begin(), needles.end(), [haystack](const auto needle) {
    return haystack.find(needle) != std::string_view::npos;
  });
}

[[nodiscard]] std::vector<std::string> categorize_imports(const PeExeResource& resource,
                                                          std::string_view category_name) {
  std::vector<std::string> matches;

  for (const auto& descriptor : resource.imports) {
    const auto dll = lower_ascii(descriptor.dll_name);

    for (const auto& symbol : descriptor.imported_functions) {
      const auto lowered_symbol = lower_ascii(symbol);
      bool selected = false;

      if (category_name == "file_io") {
        constexpr std::array<std::string_view, 12> names = {
            "createfile", "readfile", "writefile", "setfilepointer", "closehandle", "findfirstfile",
            "findnextfile", "getfileattributes", "deletefile", "movefile", "lopen", "lread"};
        selected = contains_any(lowered_symbol, names);
      } else if (category_name == "windowing_ui") {
        constexpr std::array<std::string_view, 10> names = {
            "createwindow", "dialog", "messagebox", "dispatchmessage", "peekmessage",
            "translatemessage", "showwindow", "loadcursor", "setwindow", "registerclass"};
        selected = dll == "user32.dll" || dll == "comdlg32.dll" || dll == "comctl32.dll" ||
                   contains_any(lowered_symbol, names);
      } else if (category_name == "graphics_palette") {
        constexpr std::array<std::string_view, 10> names = {"bitblt",       "stretch",      "palette",
                                                            "createdib",    "setdib",       "getdib",
                                                            "wing",         "blit",         "realizepalette",
                                                            "selectpalette"};
        selected = dll == "gdi32.dll" || dll == "wing32.dll" || contains_any(lowered_symbol, names);
      } else if (category_name == "audio_video") {
        constexpr std::array<std::string_view, 10> names = {"smack",    "wail",      "wave",
                                                            "midi",     "audio",     "video",
                                                            "play",     "sound",     "mmio",
                                                            "directsound"};
        selected = dll == "smackw32.dll" || dll == "wail32.dll" || dll == "winmm.dll" ||
                   contains_any(lowered_symbol, names);
      }

      if (selected) {
        matches.push_back(descriptor.dll_name + "!" + symbol);
      }
    }
  }

  std::sort(matches.begin(), matches.end());
  matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
  return matches;
}

}  // namespace

ParseResult<PeExeResource> parse_pe_exe_resource(std::span<const std::byte> bytes) {
  BinaryReader reader(bytes);

  const auto mz = reader.read_u16_le();
  if (!mz.ok()) {
    return {.error = mz.error};
  }
  if (mz.value.value() != 0x5A4D) {
    return {.error = make_invalid_pe_error(0, 2, bytes.size(), "Invalid DOS header signature: expected MZ")};
  }

  const auto pe_offset = read_u32_at(bytes, 0x3CU);
  if (!pe_offset.ok()) {
    return {.error = pe_offset.error};
  }

  const auto pe_header_offset = static_cast<std::size_t>(pe_offset.value.value());
  const auto pe_sig = read_u32_at(bytes, pe_header_offset);
  if (!pe_sig.ok()) {
    return {.error = make_invalid_pe_error(pe_header_offset,
                                           4,
                                           bytes.size(),
                                           "Missing PE signature at e_lfanew offset")};
  }
  if (pe_sig.value.value() != 0x00004550U) {
    return {.error = make_invalid_pe_error(pe_header_offset, 4, bytes.size(), "Invalid PE signature")};
  }

  if (const auto seek_error = reader.seek(pe_header_offset + 4); seek_error.has_value()) {
    return {.error = seek_error};
  }

  const auto machine = reader.read_u16_le();
  const auto number_of_sections = reader.read_u16_le();
  const auto timestamp = reader.read_u32_le();
  const auto ptr_to_sym = reader.read_u32_le();
  const auto num_syms = reader.read_u32_le();
  const auto size_of_optional_header = reader.read_u16_le();
  const auto characteristics = reader.read_u16_le();

  if (!machine.ok() || !number_of_sections.ok() || !timestamp.ok() || !ptr_to_sym.ok() || !num_syms.ok() ||
      !size_of_optional_header.ok() || !characteristics.ok()) {
    return {.error = make_invalid_pe_error(
                pe_header_offset + 4, 20, bytes.size(), "Truncated IMAGE_FILE_HEADER in PE executable")};
  }

  if (number_of_sections.value.value() == 0) {
    return {.error = make_invalid_pe_error(reader.tell(), 0, bytes.size(), "PE executable has zero sections")};
  }

  const auto optional_header_offset = reader.tell();
  if (size_of_optional_header.value.value() < 96) {
    return {.error = make_invalid_pe_error(optional_header_offset,
                                           size_of_optional_header.value.value(),
                                           bytes.size(),
                                           "Unsupported optional header size; need at least PE32 core fields")};
  }

  const auto optional_magic = read_u16_at(bytes, optional_header_offset);
  if (!optional_magic.ok()) {
    return {.error = optional_magic.error};
  }

  if (optional_magic.value.value() == kPe32PlusOptionalMagic) {
    return {.error = make_invalid_pe_error(optional_header_offset,
                                           2,
                                           bytes.size(),
                                           "Unsupported PE optional header: PE32+ is not supported")};
  }

  if (optional_magic.value.value() != kPe32OptionalMagic) {
    return {.error = make_invalid_pe_error(optional_header_offset,
                                           2,
                                           bytes.size(),
                                           "Unsupported PE optional header magic")};
  }

  const auto entry_point = read_u32_at(bytes, optional_header_offset + 16);
  const auto image_base = read_u32_at(bytes, optional_header_offset + 28);
  const auto subsystem = read_u16_at(bytes, optional_header_offset + 68);
  const auto number_of_rva_and_sizes = read_u32_at(bytes, optional_header_offset + 92);
  if (!entry_point.ok() || !image_base.ok() || !subsystem.ok() || !number_of_rva_and_sizes.ok()) {
    return {.error = make_invalid_pe_error(optional_header_offset,
                                           size_of_optional_header.value.value(),
                                           bytes.size(),
                                           "Truncated PE32 optional header")};
  }

  const std::size_t required_directory_count = 6;
  if (number_of_rva_and_sizes.value.value() < required_directory_count) {
    return {.error = make_invalid_pe_error(optional_header_offset + 92,
                                           4,
                                           bytes.size(),
                                           "PE does not contain required data directories")};
  }

  const auto data_directories_offset = optional_header_offset + 96;
  const auto import_rva = read_u32_at(bytes, data_directories_offset + 8);
  const auto import_size = read_u32_at(bytes, data_directories_offset + 12);
  const auto resource_rva = read_u32_at(bytes, data_directories_offset + 16);
  const auto resource_size = read_u32_at(bytes, data_directories_offset + 20);
  const auto reloc_rva = read_u32_at(bytes, data_directories_offset + 40);
  const auto reloc_size = read_u32_at(bytes, data_directories_offset + 44);

  if (!import_rva.ok() || !import_size.ok() || !resource_rva.ok() || !resource_size.ok() || !reloc_rva.ok() ||
      !reloc_size.ok()) {
    return {.error = make_invalid_pe_error(data_directories_offset,
                                           48,
                                           bytes.size(),
                                           "Truncated required PE data directory entries")};
  }

  const auto section_headers_offset = optional_header_offset + size_of_optional_header.value.value();
  if (const auto section_seek = reader.seek(section_headers_offset); section_seek.has_value()) {
    return {.error = section_seek};
  }

  std::vector<SectionHeaderView> sections;
  sections.reserve(number_of_sections.value.value());

  for (std::size_t index = 0; index < number_of_sections.value.value(); ++index) {
    const auto section_start = reader.tell();
    const auto name = reader.read_bytes(8);
    const auto virtual_size = reader.read_u32_le();
    const auto virtual_address = reader.read_u32_le();
    const auto raw_data_size = reader.read_u32_le();
    const auto raw_data_pointer = reader.read_u32_le();
    const auto ptr_reloc = reader.read_u32_le();
    const auto ptr_line = reader.read_u32_le();
    const auto num_reloc = reader.read_u16_le();
    const auto num_line = reader.read_u16_le();
    const auto section_characteristics = reader.read_u32_le();

    if (!name.ok() || !virtual_size.ok() || !virtual_address.ok() || !raw_data_size.ok() || !raw_data_pointer.ok() ||
        !ptr_reloc.ok() || !ptr_line.ok() || !num_reloc.ok() || !num_line.ok() || !section_characteristics.ok()) {
      return {.error = make_invalid_pe_error(
                  section_start, 40, bytes.size(), "Truncated IMAGE_SECTION_HEADER in PE executable")};
    }

    sections.push_back(SectionHeaderView{.virtual_address = virtual_address.value.value(),
                                         .virtual_size = virtual_size.value.value(),
                                         .raw_data_pointer = raw_data_pointer.value.value(),
                                         .raw_data_size = raw_data_size.value.value()});
  }

  PeExeResource resource;
  resource.image_base = image_base.value.value();
  resource.entry_point_rva = entry_point.value.value();
  resource.subsystem = subsystem.value.value();
  resource.section_count = number_of_sections.value.value();
  resource.timestamp = timestamp.value.value();
  resource.has_resources = resource_rva.value.value() != 0 && resource_size.value.value() != 0;
  resource.has_relocations = reloc_rva.value.value() != 0 && reloc_size.value.value() != 0;

  if (import_rva.value.value() == 0 || import_size.value.value() == 0) {
    return {.value = std::move(resource)};
  }

  const auto imports_offset = rva_to_offset(import_rva.value.value(), sections, bytes.size());
  if (!imports_offset.ok()) {
    return {.error = make_invalid_pe_error(import_rva.value.value(),
                                           import_size.value.value(),
                                           bytes.size(),
                                           "Import directory RVA did not map to a valid file region")};
  }

  std::size_t descriptor_offset = imports_offset.value.value();
  for (std::size_t descriptor_index = 0; descriptor_index < kMaxImportDescriptors; ++descriptor_index) {
    const auto original_first_thunk = read_u32_at(bytes, descriptor_offset);
    const auto time_date_stamp = read_u32_at(bytes, descriptor_offset + 4);
    const auto forwarder_chain = read_u32_at(bytes, descriptor_offset + 8);
    const auto name_rva = read_u32_at(bytes, descriptor_offset + 12);
    const auto first_thunk = read_u32_at(bytes, descriptor_offset + 16);

    if (!original_first_thunk.ok() || !time_date_stamp.ok() || !forwarder_chain.ok() || !name_rva.ok() ||
        !first_thunk.ok()) {
      return {.error = make_invalid_pe_error(
                  descriptor_offset, kImportDescriptorSize, bytes.size(), "Truncated import descriptor")};
    }

    if (original_first_thunk.value.value() == 0 && time_date_stamp.value.value() == 0 &&
        forwarder_chain.value.value() == 0 && name_rva.value.value() == 0 && first_thunk.value.value() == 0) {
      break;
    }

    const auto dll_name_offset = rva_to_offset(name_rva.value.value(), sections, bytes.size());
    if (!dll_name_offset.ok()) {
      return {.error = make_invalid_pe_error(name_rva.value.value(), 1, bytes.size(), "Import DLL name RVA is invalid")};
    }

    const auto dll_name = read_c_string_at(bytes, dll_name_offset.value.value(), "import DLL name");
    if (!dll_name.ok()) {
      return {.error = dll_name.error};
    }

    PeImportDescriptor descriptor;
    descriptor.dll_name = dll_name.value.value();

    const std::uint32_t thunk_rva = original_first_thunk.value.value() != 0 ? original_first_thunk.value.value()
                                                                             : first_thunk.value.value();
    if (thunk_rva != 0) {
      const auto thunk_offset = rva_to_offset(thunk_rva, sections, bytes.size());
      if (!thunk_offset.ok()) {
        return {.error = make_invalid_pe_error(thunk_rva, 4, bytes.size(), "Import thunk RVA is invalid")};
      }

      std::size_t current_thunk = thunk_offset.value.value();
      for (std::size_t thunk_index = 0; thunk_index < kMaxImportsPerDescriptor; ++thunk_index) {
        const auto thunk_value = read_u32_at(bytes, current_thunk);
        if (!thunk_value.ok()) {
          return {.error = make_invalid_pe_error(current_thunk, 4, bytes.size(), "Truncated import thunk table")};
        }

        const auto symbol_ref = thunk_value.value.value();
        if (symbol_ref == 0) {
          break;
        }

        if ((symbol_ref & kOrdinalFlag32) == 0) {
          const auto hint_name_offset = rva_to_offset(symbol_ref, sections, bytes.size());
          if (!hint_name_offset.ok()) {
            return {.error = make_invalid_pe_error(symbol_ref, 2, bytes.size(), "Import name RVA is invalid")};
          }

          const auto function_name = read_c_string_at(bytes, hint_name_offset.value.value() + 2, "import function name");
          if (!function_name.ok()) {
            return {.error = function_name.error};
          }

          descriptor.imported_functions.push_back(function_name.value.value());
        }

        current_thunk += 4;
      }
    }

    resource.imports.push_back(std::move(descriptor));
    descriptor_offset += kImportDescriptorSize;
  }

  return {.value = std::move(resource)};
}

ParseResult<PeExeResource> parse_pe_exe_resource(std::span<const std::uint8_t> bytes) {
  return parse_pe_exe_resource(std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
}

std::string format_pe_exe_report(const PeExeResource& resource) {
  std::ostringstream output;
  output << "# Caesar II Win95 PE EXE Report\n";
  output << "format: pe32\n";
  output << "image_base: 0x" << std::hex << std::setw(8) << std::setfill('0') << resource.image_base << std::dec << "\n";
  output << "entry_point_rva: 0x" << std::hex << std::setw(8) << std::setfill('0') << resource.entry_point_rva << std::dec
         << "\n";
  output << "subsystem: " << resource.subsystem << "\n";
  output << "section_count: " << resource.section_count << "\n";
  output << "timestamp: 0x" << std::hex << std::setw(8) << std::setfill('0') << resource.timestamp << std::dec << "\n";
  output << "has_resources: " << (resource.has_resources ? "yes" : "no") << "\n";
  output << "has_relocations: " << (resource.has_relocations ? "yes" : "no") << "\n";
  output << "import_descriptor_count: " << resource.imports.size() << "\n";

  output << "imports:\n";
  for (const auto& descriptor : resource.imports) {
    output << "  - dll: " << descriptor.dll_name << "\n";
    output << "    function_count: " << descriptor.imported_functions.size() << "\n";
    output << "    functions:\n";
    for (const auto& function : descriptor.imported_functions) {
      output << "      - " << function << "\n";
    }
  }

  output << "categorized_summary:\n";
  for (const auto category : {std::string_view("file_io"), std::string_view("windowing_ui"),
                              std::string_view("graphics_palette"), std::string_view("audio_video")}) {
    const auto entries = categorize_imports(resource, category);
    output << "  - category: " << category << "\n";
    output << "    match_count: " << entries.size() << "\n";
    output << "    matches:\n";
    for (const auto& entry : entries) {
      output << "      - " << entry << "\n";
    }
  }

  return output.str();
}

}  // namespace romulus::data
