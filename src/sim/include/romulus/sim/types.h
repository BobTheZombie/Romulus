#pragma once

#include <cstdint>

namespace romulus::sim {

struct GridPoint {
  int x = 0;
  int y = 0;

  [[nodiscard]] friend constexpr bool operator==(const GridPoint&, const GridPoint&) = default;
};

struct GridRect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

enum class TerrainType : std::uint8_t {
  Grass,
  Water,
  Rock,
};

enum class ZoneType : std::uint8_t {
  None,
  Residential,
  Commercial,
  Industrial,
};

enum class BuildingType : std::uint8_t {
  House,
  Shop,
  Workshop,
};

using BuildingId = std::uint32_t;
constexpr BuildingId k_invalid_building_id = 0;

struct Tile {
  TerrainType terrain = TerrainType::Grass;
  ZoneType zone = ZoneType::None;
  bool road = false;
  BuildingId building = k_invalid_building_id;

  [[nodiscard]] bool is_buildable() const {
    return terrain == TerrainType::Grass && !road && building == k_invalid_building_id;
  }
};

struct Building {
  BuildingId id = k_invalid_building_id;
  BuildingType type = BuildingType::House;
  GridPoint position{};
  int level = 1;
  int residents = 0;
  int jobs = 0;
};

}  // namespace romulus::sim
