#include "romulus/data/file_loader.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

std::filesystem::path make_temp_test_dir() {
  const auto unique = std::to_string(std::rand()) + "_" + std::to_string(std::rand());
  return std::filesystem::temp_directory_path() / ("romulus_file_loader_tests_" + unique);
}

int test_load_file_round_trip_bytes() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir);
  const auto file_path = temp_dir / "sample.bin";

  {
    std::ofstream output(file_path, std::ios::binary);
    output.put(static_cast<char>(0x41));
    output.put(static_cast<char>(0x42));
    output.put(static_cast<char>(0x43));
  }

  const auto loaded = romulus::data::load_file_to_memory(file_path);
  if (assert_true(loaded.ok(), "existing file should load successfully") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto& bytes = loaded.value.value().bytes;
  if (assert_true(bytes.size() == 3, "loaded bytes should match file size") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(bytes[0] == 0x41 && bytes[1] == 0x42 && bytes[2] == 0x43, "loaded bytes should preserve file data") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto reader = romulus::data::make_binary_reader(loaded.value.value());
  if (assert_true(reader.size() == 3, "binary reader should expose loaded size") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

int test_load_file_rejects_oversized_input() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir);
  const auto file_path = temp_dir / "big.bin";

  {
    std::ofstream output(file_path, std::ios::binary);
    output.write("12345", 5);
  }

  const auto loaded = romulus::data::load_file_to_memory(file_path, 4);
  if (assert_true(!loaded.ok(), "file larger than max size should fail") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(loaded.error.has_value(), "oversize failure should include error") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(loaded.error.value().code == romulus::data::FileLoadErrorCode::FileTooLarge,
                  "oversize failure should use FileTooLarge code") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

int test_load_file_rejects_directory_path() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir);

  const auto loaded = romulus::data::load_file_to_memory(temp_dir);
  if (assert_true(!loaded.ok(), "directory path should fail load") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(loaded.error.has_value(), "directory load failure should include error") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const bool matches = loaded.error.value().code == romulus::data::FileLoadErrorCode::NotRegularFile;
  if (assert_true(matches, "directory load failure should use NotRegularFile code") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

}  // namespace

int main() {
  std::srand(1337);

  if (test_load_file_round_trip_bytes() != 0) {
    return EXIT_FAILURE;
  }

  if (test_load_file_rejects_oversized_input() != 0) {
    return EXIT_FAILURE;
  }

  if (test_load_file_rejects_directory_path() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
