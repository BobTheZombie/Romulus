#include "romulus/sim/city_map.h"

#include <stdexcept>

namespace romulus::sim {

CityMap::CityMap(std::size_t width, std::size_t height)
    : width_(width), height_(height), tiles_(width * height) {
  if (width == 0 || height == 0) {
    throw std::invalid_argument("CityMap dimensions must be non-zero");
  }
}

std::size_t CityMap::width() const {
  return width_;
}

std::size_t CityMap::height() const {
  return height_;
}

bool CityMap::in_bounds(GridPoint point) const {
  return point.x >= 0 && point.y >= 0 && static_cast<std::size_t>(point.x) < width_ &&
         static_cast<std::size_t>(point.y) < height_;
}

const Tile& CityMap::at(GridPoint point) const {
  return tiles_.at(index_of(point));
}

Tile& CityMap::at(GridPoint point) {
  return tiles_.at(index_of(point));
}

const std::vector<Tile>& CityMap::tiles() const {
  return tiles_;
}

bool CityMap::set_terrain(GridPoint point, TerrainType terrain) {
  if (!in_bounds(point)) {
    return false;
  }

  Tile& tile = at(point);
  if (tile.road || tile.building != k_invalid_building_id) {
    return false;
  }

  tile.terrain = terrain;
  if (terrain != TerrainType::Grass) {
    tile.zone = ZoneType::None;
  }
  return true;
}

bool CityMap::build_road(GridPoint point) {
  if (!in_bounds(point)) {
    return false;
  }

  Tile& tile = at(point);
  if (tile.terrain != TerrainType::Grass || tile.building != k_invalid_building_id || tile.road) {
    return false;
  }

  tile.road = true;
  tile.zone = ZoneType::None;
  return true;
}

bool CityMap::remove_road(GridPoint point) {
  if (!in_bounds(point)) {
    return false;
  }

  Tile& tile = at(point);
  if (!tile.road) {
    return false;
  }

  tile.road = false;
  return true;
}

std::size_t CityMap::zone_area(GridRect area, ZoneType zone) {
  if (area.width <= 0 || area.height <= 0) {
    return 0;
  }

  std::size_t changed = 0;
  const int end_x = area.x + area.width;
  const int end_y = area.y + area.height;
  for (int y = area.y; y < end_y; ++y) {
    for (int x = area.x; x < end_x; ++x) {
      const GridPoint point{x, y};
      if (set_zone(point, zone)) {
        ++changed;
      }
    }
  }
  return changed;
}

bool CityMap::set_zone(GridPoint point, ZoneType zone) {
  if (!in_bounds(point)) {
    return false;
  }

  Tile& tile = at(point);
  if (tile.terrain != TerrainType::Grass || tile.road || tile.building != k_invalid_building_id) {
    return false;
  }

  if (tile.zone == zone) {
    return false;
  }

  tile.zone = zone;
  return true;
}

bool CityMap::has_adjacent_road(GridPoint point) const {
  for (const GridPoint neighbor : neighbors4(point)) {
    if (at(neighbor).road) {
      return true;
    }
  }
  return false;
}

std::vector<GridPoint> CityMap::neighbors4(GridPoint point) const {
  static constexpr GridPoint k_offsets[] = {
      {0, -1},
      {1, 0},
      {0, 1},
      {-1, 0},
  };

  std::vector<GridPoint> result;
  result.reserve(4);
  for (const GridPoint offset : k_offsets) {
    const GridPoint neighbor{point.x + offset.x, point.y + offset.y};
    if (in_bounds(neighbor)) {
      result.push_back(neighbor);
    }
  }
  return result;
}

bool CityMap::place_building(GridPoint point, BuildingId id) {
  if (id == k_invalid_building_id || !in_bounds(point)) {
    return false;
  }

  Tile& tile = at(point);
  if (!tile.is_buildable() || tile.zone == ZoneType::None) {
    return false;
  }

  tile.building = id;
  return true;
}

bool CityMap::remove_building(GridPoint point) {
  if (!in_bounds(point)) {
    return false;
  }

  Tile& tile = at(point);
  if (tile.building == k_invalid_building_id) {
    return false;
  }

  tile.building = k_invalid_building_id;
  return true;
}

std::size_t CityMap::index_of(GridPoint point) const {
  if (!in_bounds(point)) {
    throw std::out_of_range("CityMap point is outside map bounds");
  }
  return static_cast<std::size_t>(point.y) * width_ + static_cast<std::size_t>(point.x);
}

}  // namespace romulus::sim
