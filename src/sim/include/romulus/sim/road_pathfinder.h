#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "romulus/sim/city_map.h"

namespace romulus::sim {

[[nodiscard]] std::vector<GridPoint> find_road_path(
    const CityMap& map,
    GridPoint start,
    GridPoint goal);

[[nodiscard]] std::optional<std::size_t> road_distance(
    const CityMap& map,
    GridPoint start,
    GridPoint goal);

}  // namespace romulus::sim
