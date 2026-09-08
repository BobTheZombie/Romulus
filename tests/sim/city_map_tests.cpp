#include "romulus/sim/city_map.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    return false;
  }
  return true;
}

}  // namespace

int main() {
  using namespace romulus::sim;

  CityMap map(5, 4);
  if (!expect(map.width() == 5 && map.height() == 4, "map dimensions should be retained")) {
    return EXIT_FAILURE;
  }
  if (!expect(map.build_road({2, 1}), "road should be buildable on grass")) {
    return EXIT_FAILURE;
  }
  if (!expect(!map.set_terrain({2, 1}, TerrainType::Water), "road tile terrain should not be replaceable")) {
    return EXIT_FAILURE;
  }

  if (!expect(map.set_terrain({0, 0}, TerrainType::Water), "water terrain should be assignable")) {
    return EXIT_FAILURE;
  }
  if (!expect(!map.build_road({0, 0}), "roads should not build on water")) {
    return EXIT_FAILURE;
  }

  const std::size_t zoned = map.zone_area({1, 0, 3, 3}, ZoneType::Residential);
  if (!expect(zoned == 8, "zoning should skip the existing road tile")) {
    return EXIT_FAILURE;
  }
  if (!expect(map.has_adjacent_road({2, 0}), "zoned tile should see an adjacent road")) {
    return EXIT_FAILURE;
  }

  if (!expect(map.place_building({2, 0}, 1), "zoned grass tile should accept a building")) {
    return EXIT_FAILURE;
  }
  if (!expect(!map.set_zone({2, 0}, ZoneType::Commercial), "occupied tile should not be rezoned")) {
    return EXIT_FAILURE;
  }
  if (!expect(map.remove_building({2, 0}), "building should be removable")) {
    return EXIT_FAILURE;
  }

  bool threw = false;
  try {
    static_cast<void>(map.at({99, 99}));
  } catch (const std::out_of_range&) {
    threw = true;
  }
  if (!expect(threw, "out-of-bounds direct access should throw")) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
