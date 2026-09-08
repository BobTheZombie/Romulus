#pragma once

#include <cstddef>
#include <vector>

#include "romulus/sim/types.h"

namespace romulus::sim {

class CityMap {
 public:
  CityMap(std::size_t width, std::size_t height);

  [[nodiscard]] std::size_t width() const;
  [[nodiscard]] std::size_t height() const;
  [[nodiscard]] bool in_bounds(GridPoint point) const;

  [[nodiscard]] const Tile& at(GridPoint point) const;
  [[nodiscard]] Tile& at(GridPoint point);
  [[nodiscard]] const std::vector<Tile>& tiles() const;

  bool set_terrain(GridPoint point, TerrainType terrain);
  bool build_road(GridPoint point);
  bool remove_road(GridPoint point);
  std::size_t zone_area(GridRect area, ZoneType zone);
  bool set_zone(GridPoint point, ZoneType zone);

  [[nodiscard]] bool has_adjacent_road(GridPoint point) const;
  [[nodiscard]] std::vector<GridPoint> neighbors4(GridPoint point) const;

  bool place_building(GridPoint point, BuildingId id);
  bool remove_building(GridPoint point);

 private:
  [[nodiscard]] std::size_t index_of(GridPoint point) const;

  std::size_t width_;
  std::size_t height_;
  std::vector<Tile> tiles_;
};

}  // namespace romulus::sim
