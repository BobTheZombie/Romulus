#include "romulus/sim/road_pathfinder.h"

#include <cstdlib>
#include <iostream>

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

  CityMap map(6, 6);
  for (int x = 0; x <= 4; ++x) {
    map.build_road({x, 1});
  }
  for (int y = 1; y <= 4; ++y) {
    map.build_road({4, y});
  }

  const auto path = find_road_path(map, {0, 1}, {4, 4});
  if (!expect(path.size() == 8, "path should contain seven steps and both endpoints")) {
    return EXIT_FAILURE;
  }
  if (!expect(path.front() == GridPoint{0, 1} && path.back() == GridPoint{4, 4},
              "path should retain requested endpoints")) {
    return EXIT_FAILURE;
  }

  const auto distance = road_distance(map, {0, 1}, {4, 4});
  if (!expect(distance.has_value() && *distance == 7, "road distance should match shortest route")) {
    return EXIT_FAILURE;
  }

  map.remove_road({4, 2});
  if (!expect(find_road_path(map, {0, 1}, {4, 4}).empty(), "broken road network should be unreachable")) {
    return EXIT_FAILURE;
  }

  if (!expect(find_road_path(map, {0, 0}, {0, 1}).empty(), "path endpoints must themselves be roads")) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
