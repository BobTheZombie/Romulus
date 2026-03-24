#include "romulus/data/file_loader.h"
#include "romulus/data/image_export.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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
  return std::filesystem::temp_directory_path() / ("romulus_image_export_tests_" + unique);
}

int test_export_rgba_image_as_ppm_success() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir);
  const auto output_path = temp_dir / "tile.ppm";

  romulus::data::RgbaImage image;
  image.width = 2;
  image.height = 1;
  image.pixels_rgba = {
      255, 0, 0, 255,
      0, 255, 0, 128,
  };

  const auto exported = romulus::data::export_rgba_image_as_ppm(image, output_path);
  if (assert_true(exported.ok(), "valid RGBA image should export") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto loaded = romulus::data::load_file_to_memory(output_path);
  if (assert_true(loaded.ok(), "exported PPM should be readable") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const std::string header_prefix(reinterpret_cast<const char*>(loaded.value->bytes.data()), 11);
  if (assert_true(header_prefix == "P6\n2 1\n255\n", "PPM header should include expected magic/dimensions") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(loaded.value->bytes.size() == 17, "PPM file should include header plus 3 bytes per pixel") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  const auto& bytes = loaded.value->bytes;
  if (assert_true(bytes[11] == 255 && bytes[12] == 0 && bytes[13] == 0, "first pixel should export RGB channels") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(bytes[14] == 0 && bytes[15] == 255 && bytes[16] == 0,
                  "second pixel should export RGB channels and ignore alpha") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

int test_export_rgba_image_as_ppm_rejects_mismatched_buffer() {
  const auto temp_dir = make_temp_test_dir();
  std::filesystem::create_directories(temp_dir);
  const auto output_path = temp_dir / "bad.ppm";

  romulus::data::RgbaImage image;
  image.width = 1;
  image.height = 1;
  image.pixels_rgba = {255, 0, 0};

  const auto exported = romulus::data::export_rgba_image_as_ppm(image, output_path);
  if (assert_true(!exported.ok(), "buffer-size mismatch should fail") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(exported.error->code == romulus::data::ImageExportErrorCode::InvalidImage,
                  "buffer-size mismatch should map to InvalidImage") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  if (assert_true(!std::filesystem::exists(output_path), "failed export should not produce output file") != 0) {
    std::filesystem::remove_all(temp_dir);
    return 1;
  }

  std::filesystem::remove_all(temp_dir);
  return 0;
}

}  // namespace

int main() {
  std::srand(1337);

  if (test_export_rgba_image_as_ppm_success() != 0) {
    return EXIT_FAILURE;
  }

  if (test_export_rgba_image_as_ppm_rejects_mismatched_buffer() != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
