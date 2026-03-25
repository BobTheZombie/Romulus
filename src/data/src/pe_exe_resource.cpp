#include "romulus/data/pe_exe_resource.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <iomanip>
#include <iterator>
#include <map>
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
constexpr std::size_t kMaxResourceDirectories = 16384;
constexpr std::size_t kMaxResourceLeaves = 65536;
constexpr std::size_t kMaxVersionBlocks = 4096;
constexpr std::size_t kMaxVersionStrings = 1024;
constexpr std::size_t kVersionFixedFileInfoSize = 52;
constexpr std::uint32_t kVersionFixedFileInfoSignature = 0xFEEF04BDU;

constexpr std::uint32_t kResourceIdCursor = 1;
constexpr std::uint32_t kResourceIdBitmap = 2;
constexpr std::uint32_t kResourceIdIcon = 3;
constexpr std::uint32_t kResourceIdMenu = 4;
constexpr std::uint32_t kResourceIdDialog = 5;
constexpr std::uint32_t kResourceIdString = 6;
constexpr std::uint32_t kResourceIdFontDir = 7;
constexpr std::uint32_t kResourceIdFont = 8;
constexpr std::uint32_t kResourceIdAccelerator = 9;
constexpr std::uint32_t kResourceIdRcData = 10;
constexpr std::uint32_t kResourceIdMessageTable = 11;
constexpr std::uint32_t kResourceIdGroupCursor = 12;
constexpr std::uint32_t kResourceIdGroupIcon = 14;
constexpr std::uint32_t kResourceIdVersion = 16;
constexpr std::uint32_t kResourceIdDlgInclude = 17;
constexpr std::uint32_t kResourceIdPlugPlay = 19;
constexpr std::uint32_t kResourceIdVxd = 20;
constexpr std::uint32_t kResourceIdAniCursor = 21;
constexpr std::uint32_t kResourceIdAniIcon = 22;
constexpr std::uint32_t kResourceIdHtml = 23;
constexpr std::uint32_t kResourceIdManifest = 24;

struct SectionHeaderView {
  std::uint32_t virtual_address = 0;
  std::uint32_t virtual_size = 0;
  std::uint32_t raw_data_pointer = 0;
  std::uint32_t raw_data_size = 0;
};

struct ResourceDirectoryHeaderView {
  std::uint16_t named_entry_count = 0;
  std::uint16_t id_entry_count = 0;
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

[[nodiscard]] std::string known_resource_type_label(const std::uint32_t type_id) {
  switch (type_id) {
    case kResourceIdCursor:
      return "CURSOR";
    case kResourceIdBitmap:
      return "BITMAP";
    case kResourceIdIcon:
      return "ICON";
    case kResourceIdMenu:
      return "MENU";
    case kResourceIdDialog:
      return "DIALOG";
    case kResourceIdString:
      return "STRING";
    case kResourceIdFontDir:
      return "FONTDIR";
    case kResourceIdFont:
      return "FONT";
    case kResourceIdAccelerator:
      return "ACCELERATOR";
    case kResourceIdRcData:
      return "RCDATA";
    case kResourceIdMessageTable:
      return "MESSAGETABLE";
    case kResourceIdGroupCursor:
      return "GROUP_CURSOR";
    case kResourceIdGroupIcon:
      return "GROUP_ICON";
    case kResourceIdVersion:
      return "VERSION";
    case kResourceIdDlgInclude:
      return "DLGINCLUDE";
    case kResourceIdPlugPlay:
      return "PLUGPLAY";
    case kResourceIdVxd:
      return "VXD";
    case kResourceIdAniCursor:
      return "ANICURSOR";
    case kResourceIdAniIcon:
      return "ANIICON";
    case kResourceIdHtml:
      return "HTML";
    case kResourceIdManifest:
      return "MANIFEST";
    default:
      return "";
  }
}

[[nodiscard]] ParseResult<ResourceDirectoryHeaderView> read_resource_directory_header(
    std::span<const std::byte> bytes,
    const std::size_t absolute_offset) {
  BinaryReader reader(bytes);
  if (const auto seek_error = reader.seek(absolute_offset); seek_error.has_value()) {
    return {.error = make_invalid_pe_error(
                absolute_offset, 16, bytes.size(), "Invalid resource directory offset in PE resource section")};
  }

  const auto characteristics = reader.read_u32_le();
  const auto timestamp = reader.read_u32_le();
  const auto major = reader.read_u16_le();
  const auto minor = reader.read_u16_le();
  const auto named_entry_count = reader.read_u16_le();
  const auto id_entry_count = reader.read_u16_le();
  if (!characteristics.ok() || !timestamp.ok() || !major.ok() || !minor.ok() || !named_entry_count.ok() ||
      !id_entry_count.ok()) {
    return {.error = make_invalid_pe_error(
                absolute_offset, 16, bytes.size(), "Truncated IMAGE_RESOURCE_DIRECTORY in PE resource section")};
  }

  return {.value = ResourceDirectoryHeaderView{
              .named_entry_count = named_entry_count.value.value(),
              .id_entry_count = id_entry_count.value.value(),
          }};
}

[[nodiscard]] ParseResult<std::string> decode_resource_string_name(std::span<const std::byte> bytes,
                                                                   const std::size_t resource_section_offset,
                                                                   const std::uint32_t resource_size,
                                                                   const std::uint32_t name_offset) {
  if (name_offset >= resource_size) {
    return {.error = make_invalid_pe_error(resource_section_offset + name_offset,
                                           2,
                                           bytes.size(),
                                           "Resource string-name offset is outside resource section")};
  }

  BinaryReader reader(bytes);
  const auto string_offset = resource_section_offset + name_offset;
  if (const auto seek_error = reader.seek(string_offset); seek_error.has_value()) {
    return {.error = make_invalid_pe_error(
                string_offset, 2, bytes.size(), "Invalid resource string-name offset in PE resource section")};
  }

  const auto length_u16 = reader.read_u16_le();
  if (!length_u16.ok()) {
    return {.error = make_invalid_pe_error(string_offset, 2, bytes.size(), "Truncated resource string-name length")};
  }

  const std::size_t char_count = length_u16.value.value();
  std::string decoded;
  decoded.reserve(char_count);
  for (std::size_t index = 0; index < char_count; ++index) {
    const auto code_unit = reader.read_u16_le();
    if (!code_unit.ok()) {
      return {.error = make_invalid_pe_error(
                  string_offset, 2 + (char_count * 2), bytes.size(), "Truncated UTF-16 resource string-name data")};
    }

    const std::uint16_t value = code_unit.value.value();
    if (value >= 32U && value <= 126U) {
      decoded.push_back(static_cast<char>(value));
    } else {
      std::ostringstream escaped;
      escaped << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << value;
      decoded += escaped.str();
    }
  }

  return {.value = decoded};
}

[[nodiscard]] std::size_t align_to_4(const std::size_t value) {
  return (value + 3U) & ~static_cast<std::size_t>(3U);
}

[[nodiscard]] std::string decode_utf16_char(std::uint16_t value) {
  if (value >= 32U && value <= 126U) {
    return std::string(1, static_cast<char>(value));
  }
  std::ostringstream escaped;
  escaped << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << value;
  return escaped.str();
}

[[nodiscard]] ParseResult<std::string> read_utf16_z_string(std::span<const std::byte> bytes,
                                                           std::size_t start_offset,
                                                           std::size_t end_offset,
                                                           const std::string& field_name) {
  BinaryReader reader(bytes);
  if (const auto seek_error = reader.seek(start_offset); seek_error.has_value()) {
    return {.error = make_invalid_pe_error(start_offset, 2, bytes.size(), "Invalid " + field_name + " offset")};
  }

  std::string value;
  for (std::size_t offset = start_offset; offset + 2 <= end_offset; offset += 2) {
    const auto code_unit = reader.read_u16_le();
    if (!code_unit.ok()) {
      return {.error = make_invalid_pe_error(start_offset, 2, bytes.size(), "Truncated " + field_name)};
    }
    if (code_unit.value.value() == 0) {
      return {.value = value};
    }
    value += decode_utf16_char(code_unit.value.value());
  }

  return {.error = make_invalid_pe_error(start_offset, end_offset - start_offset, bytes.size(), "Unterminated " + field_name)};
}

[[nodiscard]] ParseResult<std::string> read_utf16_counted_string(std::span<const std::byte> bytes,
                                                                 std::size_t start_offset,
                                                                 std::size_t code_units,
                                                                 const std::string& field_name) {
  BinaryReader reader(bytes);
  if (const auto seek_error = reader.seek(start_offset); seek_error.has_value()) {
    return {.error = make_invalid_pe_error(start_offset, code_units * 2, bytes.size(), "Invalid " + field_name + " offset")};
  }

  std::string value;
  for (std::size_t index = 0; index < code_units; ++index) {
    const auto code_unit = reader.read_u16_le();
    if (!code_unit.ok()) {
      return {.error = make_invalid_pe_error(start_offset, code_units * 2, bytes.size(), "Truncated " + field_name)};
    }
    value += decode_utf16_char(code_unit.value.value());
  }
  return {.value = value};
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

[[nodiscard]] ParseResult<PeResourceSectionReport> parse_resource_section_report(
    std::span<const std::byte> bytes,
    std::span<const SectionHeaderView> sections,
    const std::uint32_t resource_rva,
    const std::uint32_t resource_size) {
  PeResourceSectionReport report;
  report.has_resources = resource_rva != 0 && resource_size != 0;
  report.resource_rva = resource_rva;
  report.resource_size = resource_size;
  if (!report.has_resources) {
    return {.value = std::move(report)};
  }

  const auto resource_section_offset = rva_to_offset(resource_rva, sections, bytes.size());
  if (!resource_section_offset.ok()) {
    return {.error = make_invalid_pe_error(
                resource_rva, resource_size, bytes.size(), "Resource directory RVA did not map to a valid file region")};
  }
  const std::size_t section_base = resource_section_offset.value.value();

  std::size_t directory_count = 0;
  auto parse_directory_entry_identity =
      [&](const std::uint32_t raw_name_field,
          const std::uint32_t raw_offset_field,
          PeResourceDirectoryEntry& out_entry,
          bool& is_named,
          std::uint32_t& numeric_id,
          std::string& string_name,
          bool& points_to_subdirectory,
          std::uint32_t& child_offset) -> std::optional<ParseError> {
    out_entry.raw_name_or_id = raw_name_field;
    is_named = (raw_name_field & 0x80000000U) != 0;
    if (is_named) {
      const auto name_offset = raw_name_field & 0x7FFFFFFFU;
      const auto decoded = decode_resource_string_name(bytes, section_base, resource_size, name_offset);
      if (!decoded.ok()) {
        return decoded.error;
      }
      string_name = decoded.value.value();
      out_entry.uses_string_name = true;
      out_entry.string_name = string_name;
      out_entry.numeric_id = 0;
    } else {
      numeric_id = raw_name_field;
      out_entry.uses_string_name = false;
      out_entry.numeric_id = numeric_id;
    }

    points_to_subdirectory = (raw_offset_field & 0x80000000U) != 0;
    child_offset = raw_offset_field & 0x7FFFFFFFU;
    if (child_offset >= resource_size) {
      return make_invalid_pe_error(section_base + child_offset,
                                   8,
                                   bytes.size(),
                                   "Resource directory entry points outside resource section");
    }
    out_entry.points_to_subdirectory = points_to_subdirectory;
    out_entry.child_offset = child_offset;
    return std::nullopt;
  };

  const auto root_header = read_resource_directory_header(bytes, section_base);
  if (!root_header.ok()) {
    return {.error = root_header.error};
  }

  const std::size_t root_entry_count = static_cast<std::size_t>(root_header.value->named_entry_count) +
                                       static_cast<std::size_t>(root_header.value->id_entry_count);
  const std::size_t root_entry_table_offset = section_base + 16;
  if (root_entry_table_offset + (root_entry_count * 8) > bytes.size()) {
    return {.error = make_invalid_pe_error(
                root_entry_table_offset, root_entry_count * 8, bytes.size(), "Truncated top-level resource directory")};
  }

  std::map<std::string, std::size_t> per_type_entry_counts;
  std::map<std::string, std::size_t> per_type_leaf_counts;

  for (std::size_t type_index = 0; type_index < root_entry_count; ++type_index) {
    if (++directory_count > kMaxResourceDirectories) {
      return {.error = make_invalid_pe_error(
                  section_base, resource_size, bytes.size(), "Exceeded bounded resource directory traversal limit")};
    }

    const std::size_t type_entry_offset = root_entry_table_offset + (type_index * 8);
    const auto type_name_or_id = read_u32_at(bytes, type_entry_offset);
    const auto type_data = read_u32_at(bytes, type_entry_offset + 4);
    if (!type_name_or_id.ok() || !type_data.ok()) {
      return {.error = make_invalid_pe_error(type_entry_offset, 8, bytes.size(), "Truncated resource type entry")};
    }

    PeResourceDirectoryEntry top_level_entry;
    bool type_named = false;
    std::uint32_t type_id = 0;
    std::string type_string_name;
    bool type_points_to_subdirectory = false;
    std::uint32_t type_child_offset = 0;
    if (const auto maybe_error = parse_directory_entry_identity(type_name_or_id.value.value(),
                                                                 type_data.value.value(),
                                                                 top_level_entry,
                                                                 type_named,
                                                                 type_id,
                                                                 type_string_name,
                                                                 type_points_to_subdirectory,
                                                                 type_child_offset);
        maybe_error.has_value()) {
      return {.error = maybe_error};
    }

    if (!type_points_to_subdirectory) {
      return {.error = make_invalid_pe_error(type_entry_offset,
                                             8,
                                             bytes.size(),
                                             "Top-level resource type entry must point to a subdirectory")};
    }

    report.tree.top_level_entries.push_back(top_level_entry);
    const auto type_label = type_named ? std::string() : known_resource_type_label(type_id);
    const auto type_key = type_named ? std::string("name:") + type_string_name
                                     : std::string("id:") + std::to_string(type_id);

    const std::size_t type_dir_offset = section_base + type_child_offset;
    const auto type_dir_header = read_resource_directory_header(bytes, type_dir_offset);
    if (!type_dir_header.ok()) {
      return {.error = type_dir_header.error};
    }

    const std::size_t type_name_count = static_cast<std::size_t>(type_dir_header.value->named_entry_count) +
                                        static_cast<std::size_t>(type_dir_header.value->id_entry_count);
    per_type_entry_counts[type_key] = type_name_count;
    const std::size_t type_entries_offset = type_dir_offset + 16;

    for (std::size_t name_index = 0; name_index < type_name_count; ++name_index) {
      if (++directory_count > kMaxResourceDirectories) {
        return {.error = make_invalid_pe_error(
                    section_base, resource_size, bytes.size(), "Exceeded bounded resource directory traversal limit")};
      }

      const std::size_t name_entry_offset = type_entries_offset + (name_index * 8);
      const auto name_name_or_id = read_u32_at(bytes, name_entry_offset);
      const auto name_data = read_u32_at(bytes, name_entry_offset + 4);
      if (!name_name_or_id.ok() || !name_data.ok()) {
        return {.error = make_invalid_pe_error(name_entry_offset, 8, bytes.size(), "Truncated resource name entry")};
      }

      PeResourceDirectoryEntry name_entry;
      bool name_named = false;
      std::uint32_t name_id = 0;
      std::string name_string;
      bool name_points_to_subdirectory = false;
      std::uint32_t name_child_offset = 0;
      if (const auto maybe_error = parse_directory_entry_identity(name_name_or_id.value.value(),
                                                                   name_data.value.value(),
                                                                   name_entry,
                                                                   name_named,
                                                                   name_id,
                                                                   name_string,
                                                                   name_points_to_subdirectory,
                                                                   name_child_offset);
          maybe_error.has_value()) {
        return {.error = maybe_error};
      }

      std::size_t language_count = 0;
      std::size_t language_entries_offset = 0;
      bool has_explicit_language_directory = false;
      if (name_points_to_subdirectory) {
        const std::size_t language_dir_offset = section_base + name_child_offset;
        const auto language_dir_header = read_resource_directory_header(bytes, language_dir_offset);
        if (!language_dir_header.ok()) {
          return {.error = language_dir_header.error};
        }
        language_count = static_cast<std::size_t>(language_dir_header.value->named_entry_count) +
                         static_cast<std::size_t>(language_dir_header.value->id_entry_count);
        language_entries_offset = language_dir_offset + 16;
        has_explicit_language_directory = true;
      } else {
        language_count = 1;
      }

      for (std::size_t language_index = 0; language_index < language_count; ++language_index) {
        if (report.tree.leaves.size() >= kMaxResourceLeaves) {
          return {.error = make_invalid_pe_error(
                      section_base, resource_size, bytes.size(), "Exceeded bounded resource leaf traversal limit")};
        }

        std::uint32_t raw_language_name = 0;
        std::uint32_t raw_data_entry = 0;
        if (has_explicit_language_directory) {
          const std::size_t language_entry_offset = language_entries_offset + (language_index * 8);
          const auto language_name_or_id = read_u32_at(bytes, language_entry_offset);
          const auto language_data = read_u32_at(bytes, language_entry_offset + 4);
          if (!language_name_or_id.ok() || !language_data.ok()) {
            return {.error = make_invalid_pe_error(
                        language_entry_offset, 8, bytes.size(), "Truncated resource language entry")};
          }

          raw_language_name = language_name_or_id.value.value();
          if ((raw_language_name & 0x80000000U) != 0) {
            return {.error = make_invalid_pe_error(
                        language_entry_offset, 8, bytes.size(), "Language resource entries must use numeric language IDs")};
          }

          raw_data_entry = language_data.value.value();
          if ((raw_data_entry & 0x80000000U) != 0) {
            return {.error = make_invalid_pe_error(
                        language_entry_offset, 8, bytes.size(), "Language resource entry points to nested directory")};
          }
        } else {
          raw_language_name = 0;
          raw_data_entry = name_child_offset;
        }
        if (raw_data_entry >= resource_size) {
          return {.error = make_invalid_pe_error(
                      section_base + raw_data_entry, 16, bytes.size(), "Resource data entry offset is out of range")};
        }

        const std::size_t data_entry_offset = section_base + raw_data_entry;
        const auto data_rva = read_u32_at(bytes, data_entry_offset);
        const auto data_size = read_u32_at(bytes, data_entry_offset + 4);
        const auto code_page = read_u32_at(bytes, data_entry_offset + 8);
        const auto reserved = read_u32_at(bytes, data_entry_offset + 12);
        if (!data_rva.ok() || !data_size.ok() || !code_page.ok() || !reserved.ok()) {
          return {.error = make_invalid_pe_error(
                      data_entry_offset, 16, bytes.size(), "Truncated IMAGE_RESOURCE_DATA_ENTRY")};
        }
        if (data_rva.value.value() == 0 || data_size.value.value() == 0) {
          continue;
        }

        const auto data_file_offset = rva_to_offset(data_rva.value.value(), sections, bytes.size());
        if (!data_file_offset.ok()) {
          return {.error = make_invalid_pe_error(
                      data_rva.value.value(), data_size.value.value(), bytes.size(), "Resource data RVA is invalid")};
        }

        const std::uint64_t end_offset =
            static_cast<std::uint64_t>(data_file_offset.value.value()) + static_cast<std::uint64_t>(data_size.value.value());
        if (end_offset > bytes.size()) {
          return {.error = make_invalid_pe_error(data_file_offset.value.value(),
                                                 data_size.value.value(),
                                                 bytes.size(),
                                                 "Resource data payload extends beyond available file bytes")};
        }

        PeResourceLeaf leaf;
        leaf.type_id = type_id;
        leaf.type_label = type_label;
        leaf.type_uses_string_name = type_named;
        leaf.type_string_name = type_string_name;
        leaf.name_id = name_id;
        leaf.name_uses_string_name = name_named;
        leaf.name_string = name_string;
        leaf.language_id = raw_language_name;
        leaf.data_rva = data_rva.value.value();
        leaf.data_size = data_size.value.value();
        leaf.data_file_offset = data_file_offset.value.value();
        report.tree.leaves.push_back(std::move(leaf));
        per_type_leaf_counts[type_key] += 1;
      }
    }
  }

  std::sort(report.tree.top_level_entries.begin(),
            report.tree.top_level_entries.end(),
            [](const PeResourceDirectoryEntry& lhs, const PeResourceDirectoryEntry& rhs) {
              if (lhs.uses_string_name != rhs.uses_string_name) {
                return !lhs.uses_string_name;
              }
              if (lhs.uses_string_name) {
                return lhs.string_name < rhs.string_name;
              }
              return lhs.numeric_id < rhs.numeric_id;
            });

  std::sort(report.tree.leaves.begin(), report.tree.leaves.end(), [](const PeResourceLeaf& lhs, const PeResourceLeaf& rhs) {
    const auto lhs_type_key = lhs.type_uses_string_name ? lhs.type_string_name : std::to_string(lhs.type_id);
    const auto rhs_type_key = rhs.type_uses_string_name ? rhs.type_string_name : std::to_string(rhs.type_id);
    if (lhs_type_key != rhs_type_key) {
      return lhs_type_key < rhs_type_key;
    }
    const auto lhs_name_key = lhs.name_uses_string_name ? lhs.name_string : std::to_string(lhs.name_id);
    const auto rhs_name_key = rhs.name_uses_string_name ? rhs.name_string : std::to_string(rhs.name_id);
    if (lhs_name_key != rhs_name_key) {
      return lhs_name_key < rhs_name_key;
    }
    return lhs.language_id < rhs.language_id;
  });

  for (const auto& entry : report.tree.top_level_entries) {
    PeResourceTypeSummary summary;
    summary.type_id = entry.numeric_id;
    summary.type_label = known_resource_type_label(entry.numeric_id);
    summary.type_uses_string_name = entry.uses_string_name;
    summary.type_string_name = entry.string_name;

    const auto type_key = entry.uses_string_name ? std::string("name:") + entry.string_name
                                                 : std::string("id:") + std::to_string(entry.numeric_id);
    summary.entry_count = per_type_entry_counts[type_key];
    summary.leaf_count = per_type_leaf_counts[type_key];
    report.per_type_summary.push_back(std::move(summary));
  }

  report.top_level_type_count = report.tree.top_level_entries.size();
  report.leaf_count = report.tree.leaves.size();
  return {.value = std::move(report)};
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
  const auto resource_report =
      parse_resource_section_report(bytes, sections, resource_rva.value.value(), resource_size.value.value());
  if (!resource_report.ok()) {
    return {.error = resource_report.error};
  }
  resource.resource_report = resource_report.value.value();

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

ParseResult<PeResourcePayloadReport> decode_pe_resource_payloads(std::span<const std::byte> bytes,
                                                                 const PeExeResource& resource) {
  PeResourcePayloadReport report;
  for (const auto& leaf : resource.resource_report.tree.leaves) {
    const std::size_t payload_size = static_cast<std::size_t>(leaf.data_size);
    if (leaf.data_file_offset + payload_size > bytes.size()) {
      return {.error = make_invalid_pe_error(
                  leaf.data_file_offset, payload_size, bytes.size(), "Resource payload points outside file bytes")};
    }
    const auto payload = bytes.subspan(leaf.data_file_offset, payload_size);

    if (!leaf.type_uses_string_name && leaf.type_id == kResourceIdVersion) {
      if (payload.size() < 6) {
        return {.error = make_invalid_pe_error(
                    leaf.data_file_offset, payload.size(), bytes.size(), "Truncated VERSION resource header")};
      }

      BinaryReader reader(payload);
      const auto root_length = reader.read_u16_le();
      const auto root_value_length = reader.read_u16_le();
      const auto root_type = reader.read_u16_le();
      if (!root_length.ok() || !root_value_length.ok() || !root_type.ok()) {
        return {.error = make_invalid_pe_error(
                    leaf.data_file_offset, payload.size(), bytes.size(), "Truncated VERSION resource root header")};
      }
      const std::size_t declared_root_length = root_length.value.value();
      if (declared_root_length < 6 || declared_root_length > payload.size()) {
        return {.error = make_invalid_pe_error(leaf.data_file_offset,
                                               payload.size(),
                                               bytes.size(),
                                               "Invalid VS_VERSION_INFO root length in VERSION resource")};
      }

      const auto root_key = read_utf16_z_string(payload, 6, declared_root_length, "VERSION root key");
      if (!root_key.ok()) {
        return {.error = make_invalid_pe_error(
                    leaf.data_file_offset, declared_root_length, bytes.size(), root_key.error->message)};
      }
      if (root_key.value.value() != "VS_VERSION_INFO") {
        return {.error = make_invalid_pe_error(leaf.data_file_offset,
                                               declared_root_length,
                                               bytes.size(),
                                               "Unsupported VERSION root key; expected VS_VERSION_INFO")};
      }

      std::size_t key_bytes = (root_key.value->size() + 1U) * 2U;
      std::size_t value_offset = align_to_4(6U + key_bytes);
      if (value_offset > declared_root_length) {
        return {.error = make_invalid_pe_error(
                    leaf.data_file_offset, declared_root_length, bytes.size(), "Invalid VERSION key alignment")};
      }

      PeVersionResource decoded;
      decoded.source_leaf = leaf;

      const std::size_t root_value_byte_length = static_cast<std::size_t>(root_value_length.value.value()) * 2U;
      if (root_type.value.value() == 0 && root_value_byte_length >= kVersionFixedFileInfoSize &&
          value_offset + kVersionFixedFileInfoSize <= declared_root_length) {
        BinaryReader fixed_reader(payload);
        if (const auto seek_error = fixed_reader.seek(value_offset); seek_error.has_value()) {
          return {.error = make_invalid_pe_error(
                      leaf.data_file_offset + value_offset, 4, bytes.size(), "Invalid VERSION fixed file info offset")};
        }
        const auto signature = fixed_reader.read_u32_le();
        if (!signature.ok()) {
          return {.error = make_invalid_pe_error(
                      leaf.data_file_offset + value_offset, 4, bytes.size(), "Truncated VERSION fixed file info")};
        }
        if (signature.value.value() == kVersionFixedFileInfoSignature) {
          decoded.has_fixed_file_info = true;
          const auto skip_struc = fixed_reader.read_u32_le();
          const auto file_ms = fixed_reader.read_u32_le();
          const auto file_ls = fixed_reader.read_u32_le();
          const auto product_ms = fixed_reader.read_u32_le();
          const auto product_ls = fixed_reader.read_u32_le();
          if (!skip_struc.ok() || !file_ms.ok() || !file_ls.ok() || !product_ms.ok() || !product_ls.ok()) {
            return {.error = make_invalid_pe_error(leaf.data_file_offset + value_offset,
                                                   kVersionFixedFileInfoSize,
                                                   bytes.size(),
                                                   "Truncated VS_FIXEDFILEINFO in VERSION resource")};
          }
          decoded.file_version_ms = file_ms.value.value();
          decoded.file_version_ls = file_ls.value.value();
          decoded.product_version_ms = product_ms.value.value();
          decoded.product_version_ls = product_ls.value.value();
        }
      }

      std::vector<std::size_t> block_offsets;
      block_offsets.push_back(0);
      for (std::size_t block_index = 0; block_index < block_offsets.size(); ++block_index) {
        if (block_index > kMaxVersionBlocks) {
          return {.error = make_invalid_pe_error(
                      leaf.data_file_offset, payload.size(), bytes.size(), "Exceeded VERSION block traversal limit")};
        }

        const std::size_t block_offset = block_offsets[block_index];
        if (block_offset + 6 > declared_root_length) {
          continue;
        }
        BinaryReader block_reader(payload);
        if (const auto seek_error = block_reader.seek(block_offset); seek_error.has_value()) {
          return {.error = make_invalid_pe_error(
                      leaf.data_file_offset + block_offset, 6, bytes.size(), "Invalid VERSION block offset")};
        }
        const auto block_length = block_reader.read_u16_le();
        const auto block_value_length = block_reader.read_u16_le();
        const auto block_type = block_reader.read_u16_le();
        if (!block_length.ok() || !block_value_length.ok() || !block_type.ok()) {
          return {.error = make_invalid_pe_error(
                      leaf.data_file_offset + block_offset, 6, bytes.size(), "Truncated VERSION block header")};
        }
        const std::size_t length = block_length.value.value();
        if (length < 6) {
          continue;
        }
        const std::size_t block_end = block_offset + length;
        if (block_end > declared_root_length) {
          return {.error = make_invalid_pe_error(
                      leaf.data_file_offset + block_offset, length, bytes.size(), "VERSION block extends beyond root")};
        }

        const auto block_key = read_utf16_z_string(payload, block_offset + 6, block_end, "VERSION block key");
        if (!block_key.ok()) {
          return {.error = make_invalid_pe_error(
                      leaf.data_file_offset + block_offset, length, bytes.size(), block_key.error->message)};
        }
        const std::size_t block_key_bytes = (block_key.value->size() + 1U) * 2U;
        const std::size_t block_value_offset = block_offset + align_to_4(6U + block_key_bytes);

        const bool can_decode_string =
            block_type.value.value() == 1 && block_value_length.value.value() > 0 &&
            block_value_offset + (static_cast<std::size_t>(block_value_length.value.value()) * 2U) <= block_end;
        if (can_decode_string && block_key.value.value() != "VS_VERSION_INFO") {
          const auto value = read_utf16_counted_string(payload,
                                                       block_value_offset,
                                                       block_value_length.value.value(),
                                                       "VERSION string value");
          if (!value.ok()) {
            return {.error = make_invalid_pe_error(
                        leaf.data_file_offset + block_value_offset,
                        static_cast<std::size_t>(block_value_length.value.value()) * 2U,
                        bytes.size(),
                        value.error->message)};
          }
          decoded.string_values.push_back({.key = block_key.value.value(), .value = value.value.value()});
          if (decoded.string_values.size() > kMaxVersionStrings) {
            return {.error = make_invalid_pe_error(
                        leaf.data_file_offset, payload.size(), bytes.size(), "Exceeded VERSION string extraction limit")};
          }
        }

        std::size_t child_offset = block_offset + align_to_4(6U + block_key_bytes +
                                                              (static_cast<std::size_t>(block_value_length.value.value()) *
                                                               (block_type.value.value() == 1 ? 2U : 1U)));
        while (child_offset + 6 <= block_end) {
          BinaryReader child_reader(payload);
          if (const auto seek_error = child_reader.seek(child_offset); seek_error.has_value()) {
            break;
          }
          const auto child_length = child_reader.read_u16_le();
          if (!child_length.ok()) {
            break;
          }
          const std::size_t child_size = child_length.value.value();
          if (child_size == 0) {
            break;
          }
          if (child_offset + child_size > block_end) {
            return {.error = make_invalid_pe_error(leaf.data_file_offset + child_offset,
                                                   child_size,
                                                   bytes.size(),
                                                   "VERSION child block exceeds parent bounds")};
          }
          block_offsets.push_back(child_offset);
          child_offset += align_to_4(child_size);
        }
      }

      std::sort(decoded.string_values.begin(),
                decoded.string_values.end(),
                [](const PeVersionResourceStringValue& lhs, const PeVersionResourceStringValue& rhs) {
                  if (lhs.key != rhs.key) {
                    return lhs.key < rhs.key;
                  }
                  return lhs.value < rhs.value;
                });
      report.version_resources.push_back(std::move(decoded));
      continue;
    }

    if (!leaf.type_uses_string_name && leaf.type_id == kResourceIdString) {
      PeStringTableResource table;
      table.source_leaf = leaf;
      BinaryReader reader(payload);
      for (std::size_t index = 0; index < 16; ++index) {
        const auto char_count = reader.read_u16_le();
        if (!char_count.ok()) {
          return {.error = make_invalid_pe_error(
                      leaf.data_file_offset, payload.size(), bytes.size(), "Truncated STRINGTABLE string length")};
        }
        const std::size_t chars = char_count.value.value();
        const auto text = read_utf16_counted_string(payload, reader.tell(), chars, "STRINGTABLE string");
        if (!text.ok()) {
          return {.error = make_invalid_pe_error(
                      leaf.data_file_offset, payload.size(), bytes.size(), "Truncated STRINGTABLE string payload")};
        }
        if (chars > 0) {
          const std::uint32_t base_id = leaf.name_id == 0 ? 0 : ((leaf.name_id - 1U) * 16U);
          table.entries.push_back({.string_id = base_id + static_cast<std::uint32_t>(index), .text = text.value.value()});
        }
        if (const auto skip_error = reader.seek(reader.tell() + (chars * 2U)); skip_error.has_value()) {
          return {.error = make_invalid_pe_error(
                      leaf.data_file_offset, payload.size(), bytes.size(), "Truncated STRINGTABLE string payload")};
        }
      }
      report.string_table_resources.push_back(std::move(table));
      continue;
    }

    report.skipped_resources.push_back({.source_leaf = leaf, .reason = "unsupported resource type"});
  }

  std::sort(report.version_resources.begin(),
            report.version_resources.end(),
            [](const PeVersionResource& lhs, const PeVersionResource& rhs) {
              if (lhs.source_leaf.name_id != rhs.source_leaf.name_id) {
                return lhs.source_leaf.name_id < rhs.source_leaf.name_id;
              }
              return lhs.source_leaf.language_id < rhs.source_leaf.language_id;
            });
  std::sort(report.string_table_resources.begin(),
            report.string_table_resources.end(),
            [](const PeStringTableResource& lhs, const PeStringTableResource& rhs) {
              if (lhs.source_leaf.name_id != rhs.source_leaf.name_id) {
                return lhs.source_leaf.name_id < rhs.source_leaf.name_id;
              }
              return lhs.source_leaf.language_id < rhs.source_leaf.language_id;
            });
  return {.value = std::move(report)};
}

ParseResult<PeResourcePayloadReport> decode_pe_resource_payloads(std::span<const std::uint8_t> bytes,
                                                                 const PeExeResource& resource) {
  return decode_pe_resource_payloads(
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()), resource);
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
  output << "resource_type_count: " << resource.resource_report.top_level_type_count << "\n";
  output << "resource_leaf_count: " << resource.resource_report.leaf_count << "\n";
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

std::string format_pe_resource_report(const PeResourceSectionReport& resource_report) {
  std::ostringstream output;
  output << "# Caesar II Win95 PE Resource Report\n";
  output << "has_resources: " << (resource_report.has_resources ? "yes" : "no") << "\n";
  output << "resource_rva: 0x" << std::hex << std::setw(8) << std::setfill('0') << resource_report.resource_rva
         << std::dec << "\n";
  output << "resource_size: " << resource_report.resource_size << "\n";
  output << "top_level_type_count: " << resource_report.top_level_type_count << "\n";
  output << "leaf_count: " << resource_report.leaf_count << "\n";
  output << "per_type_summary:\n";
  for (const auto& type : resource_report.per_type_summary) {
    output << "  - type: ";
    if (type.type_uses_string_name) {
      output << "name:" << type.type_string_name << "\n";
    } else {
      output << "id:" << type.type_id;
      if (!type.type_label.empty()) {
        output << " (" << type.type_label << ")";
      }
      output << "\n";
    }
    output << "    entry_count: " << type.entry_count << "\n";
    output << "    leaf_count: " << type.leaf_count << "\n";
  }

  output << "leaf_entries:\n";
  for (const auto& leaf : resource_report.tree.leaves) {
    output << "  - type: ";
    if (leaf.type_uses_string_name) {
      output << "name:" << leaf.type_string_name << "\n";
    } else {
      output << "id:" << leaf.type_id;
      if (!leaf.type_label.empty()) {
        output << " (" << leaf.type_label << ")";
      }
      output << "\n";
    }
    output << "    name: " << (leaf.name_uses_string_name ? "name:" + leaf.name_string
                                                           : "id:" + std::to_string(leaf.name_id))
           << "\n";
    output << "    language_id: " << leaf.language_id << "\n";
    output << "    data_rva: 0x" << std::hex << std::setw(8) << std::setfill('0') << leaf.data_rva << std::dec << "\n";
    output << "    data_file_offset: " << leaf.data_file_offset << "\n";
    output << "    data_size: " << leaf.data_size << "\n";
  }

  return output.str();
}

std::string format_pe_version_resource_report(const PeResourcePayloadReport& payload_report) {
  std::ostringstream output;
  output << "version_resource_count: " << payload_report.version_resources.size() << "\n";
  output << "version_resources:\n";
  for (const auto& version : payload_report.version_resources) {
    output << "  - name_id: " << version.source_leaf.name_id << "\n";
    output << "    language_id: " << version.source_leaf.language_id << "\n";
    output << "    has_fixed_file_info: " << (version.has_fixed_file_info ? "yes" : "no") << "\n";
    if (version.has_fixed_file_info) {
      output << "    file_version_ms: 0x" << std::hex << std::setw(8) << std::setfill('0') << version.file_version_ms
             << std::dec << "\n";
      output << "    file_version_ls: 0x" << std::hex << std::setw(8) << std::setfill('0') << version.file_version_ls
             << std::dec << "\n";
      output << "    product_version_ms: 0x" << std::hex << std::setw(8) << std::setfill('0')
             << version.product_version_ms << std::dec << "\n";
      output << "    product_version_ls: 0x" << std::hex << std::setw(8) << std::setfill('0')
             << version.product_version_ls << std::dec << "\n";
    }
    output << "    string_values:\n";
    for (const auto& pair : version.string_values) {
      output << "      - key: " << pair.key << "\n";
      output << "        value: " << pair.value << "\n";
    }
  }
  return output.str();
}

std::string format_pe_string_table_report(const PeResourcePayloadReport& payload_report) {
  std::ostringstream output;
  output << "string_table_resource_count: " << payload_report.string_table_resources.size() << "\n";
  output << "string_tables:\n";
  for (const auto& table : payload_report.string_table_resources) {
    output << "  - name_id: " << table.source_leaf.name_id << "\n";
    output << "    language_id: " << table.source_leaf.language_id << "\n";
    output << "    entries:\n";
    for (const auto& entry : table.entries) {
      output << "      - id: " << entry.string_id << "\n";
      output << "        text: " << entry.text << "\n";
    }
  }
  return output.str();
}

std::string format_pe_resource_payload_report(const PeResourcePayloadReport& payload_report) {
  std::ostringstream output;
  output << "# Caesar II Win95 PE Resource Payload Report\n";
  output << format_pe_version_resource_report(payload_report);
  output << format_pe_string_table_report(payload_report);
  output << "skipped_resource_count: " << payload_report.skipped_resources.size() << "\n";
  output << "skipped_resources:\n";
  for (const auto& skipped : payload_report.skipped_resources) {
    output << "  - type: ";
    if (skipped.source_leaf.type_uses_string_name) {
      output << "name:" << skipped.source_leaf.type_string_name << "\n";
    } else {
      output << "id:" << skipped.source_leaf.type_id;
      if (!skipped.source_leaf.type_label.empty()) {
        output << " (" << skipped.source_leaf.type_label << ")";
      }
      output << "\n";
    }
    output << "    name: " << (skipped.source_leaf.name_uses_string_name ? "name:" + skipped.source_leaf.name_string
                                                                          : "id:" + std::to_string(skipped.source_leaf.name_id))
           << "\n";
    output << "    language_id: " << skipped.source_leaf.language_id << "\n";
    output << "    reason: " << skipped.reason << "\n";
  }
  return output.str();
}

}  // namespace romulus::data
