#include "romulus/sim/city_simulation.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace romulus::sim {
namespace {

int clamp_demand(int value) {
  return std::clamp(value, -100, 100);
}

Building building_for_zone(BuildingId id, ZoneType zone, GridPoint point) {
  switch (zone) {
    case ZoneType::Residential:
      return Building{id, BuildingType::House, point, 1, 5, 0};
    case ZoneType::Commercial:
      return Building{id, BuildingType::Shop, point, 1, 0, 4};
    case ZoneType::Industrial:
      return Building{id, BuildingType::Workshop, point, 1, 0, 6};
    case ZoneType::None:
      break;
  }
  return Building{id, BuildingType::House, point, 1, 0, 0};
}

bool demand_allows_growth(const DemandState& demand, ZoneType zone) {
  switch (zone) {
    case ZoneType::Residential:
      return demand.residential > 0;
    case ZoneType::Commercial:
      return demand.commercial > 0;
    case ZoneType::Industrial:
      return demand.industrial > 0;
    case ZoneType::None:
      return false;
  }
  return false;
}

void hash_byte(std::uint64_t& hash, std::uint8_t value) {
  constexpr std::uint64_t k_prime = 1099511628211ULL;
  hash ^= value;
  hash *= k_prime;
}

template <typename Integer>
void hash_integer(std::uint64_t& hash, Integer value) {
  using Unsigned = std::make_unsigned_t<Integer>;
  Unsigned unsigned_value = static_cast<Unsigned>(value);
  for (std::size_t index = 0; index < sizeof(Unsigned); ++index) {
    hash_byte(hash, static_cast<std::uint8_t>((unsigned_value >> (index * 8)) & 0xffU));
  }
}

}  // namespace

CitySimulation::CitySimulation(std::size_t width, std::size_t height, SimulationConfig config)
    : map_(width, height), config_(config), treasury_(config.initial_treasury) {
  if (config_.growth_interval_ticks == 0) {
    config_.growth_interval_ticks = 1;
  }
  if (config_.economy_interval_ticks == 0) {
    config_.economy_interval_ticks = 1;
  }
  refresh_cached_stats();
}

const CityMap& CitySimulation::map() const {
  return map_;
}

const std::vector<Building>& CitySimulation::buildings() const {
  return buildings_;
}

const Building* CitySimulation::building_at(GridPoint point) const {
  if (!map_.in_bounds(point)) {
    return nullptr;
  }

  const BuildingId id = map_.at(point).building;
  if (id == k_invalid_building_id) {
    return nullptr;
  }

  const auto found = std::find_if(buildings_.begin(), buildings_.end(), [id](const Building& building) {
    return building.id == id;
  });
  return found == buildings_.end() ? nullptr : &*found;
}

CityStats CitySimulation::stats() const {
  return cached_stats_;
}

std::uint64_t CitySimulation::state_hash() const {
  std::uint64_t hash = 1469598103934665603ULL;
  hash_integer(hash, tick_);
  hash_integer(hash, treasury_);
  hash_integer(hash, last_month_balance_);
  hash_integer(hash, next_building_id_);
  hash_integer(hash, map_.width());
  hash_integer(hash, map_.height());

  for (const Tile& tile : map_.tiles()) {
    hash_byte(hash, static_cast<std::uint8_t>(tile.terrain));
    hash_byte(hash, static_cast<std::uint8_t>(tile.zone));
    hash_byte(hash, tile.road ? 1U : 0U);
    hash_integer(hash, tile.building);
  }

  for (const Building& building : buildings_) {
    hash_integer(hash, building.id);
    hash_byte(hash, static_cast<std::uint8_t>(building.type));
    hash_integer(hash, building.position.x);
    hash_integer(hash, building.position.y);
    hash_integer(hash, building.level);
    hash_integer(hash, building.residents);
    hash_integer(hash, building.jobs);
  }

  return hash;
}

bool CitySimulation::set_terrain(GridPoint point, TerrainType terrain) {
  const bool changed = map_.set_terrain(point, terrain);
  if (changed) {
    refresh_cached_stats();
  }
  return changed;
}

bool CitySimulation::build_road(GridPoint point) {
  if (!can_afford(config_.road_build_cost) || !map_.build_road(point)) {
    return false;
  }

  treasury_ -= config_.road_build_cost;
  refresh_cached_stats();
  return true;
}

std::size_t CitySimulation::zone(GridRect area, ZoneType zone_type) {
  const std::size_t candidate_count = zoneable_tile_count(area, zone_type);
  if (candidate_count == 0) {
    return 0;
  }

  const std::int64_t total_cost = static_cast<std::int64_t>(candidate_count) *
                                  static_cast<std::int64_t>(config_.zoning_cost_per_tile);
  if (total_cost > std::numeric_limits<int>::max() || !can_afford(static_cast<int>(total_cost))) {
    return 0;
  }

  const std::size_t changed = map_.zone_area(area, zone_type);
  treasury_ -= static_cast<int>(changed) * config_.zoning_cost_per_tile;
  refresh_cached_stats();
  return changed;
}

bool CitySimulation::bulldoze(GridPoint point) {
  if (!map_.in_bounds(point) || !can_afford(config_.bulldoze_cost)) {
    return false;
  }

  Tile& tile = map_.at(point);
  bool changed = false;
  if (tile.building != k_invalid_building_id) {
    const BuildingId id = tile.building;
    changed = map_.remove_building(point);
    if (changed) {
      remove_building_by_id(id);
    }
  } else if (tile.road) {
    changed = map_.remove_road(point);
  } else if (tile.zone != ZoneType::None) {
    changed = map_.set_zone(point, ZoneType::None);
  }

  if (!changed) {
    return false;
  }

  treasury_ -= config_.bulldoze_cost;
  refresh_cached_stats();
  return true;
}

void CitySimulation::advance_tick() {
  ++tick_;

  if (tick_ % config_.growth_interval_ticks == 0) {
    grow_one_building();
  }

  if (tick_ % config_.economy_interval_ticks == 0) {
    process_month();
  }

  refresh_cached_stats();
}

void CitySimulation::advance_ticks(std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    advance_tick();
  }
}

DemandState CitySimulation::calculate_demand() const {
  const int city_population = population();
  int commercial_jobs = 0;
  int industrial_jobs = 0;
  for (const Building& building : buildings_) {
    if (building.type == BuildingType::Shop) {
      commercial_jobs += building.jobs;
    } else if (building.type == BuildingType::Workshop) {
      industrial_jobs += building.jobs;
    }
  }

  DemandState demand;
  demand.residential = clamp_demand(25 + jobs() * 2 - city_population);
  demand.commercial = clamp_demand(city_population * 2 - commercial_jobs * 4);
  demand.industrial = clamp_demand(city_population * 2 - industrial_jobs * 3);
  return demand;
}

int CitySimulation::population() const {
  int result = 0;
  for (const Building& building : buildings_) {
    result += building.residents;
  }
  return result;
}

int CitySimulation::jobs() const {
  int result = 0;
  for (const Building& building : buildings_) {
    result += building.jobs;
  }
  return result;
}

std::size_t CitySimulation::road_tile_count() const {
  return static_cast<std::size_t>(std::count_if(map_.tiles().begin(), map_.tiles().end(), [](const Tile& tile) {
    return tile.road;
  }));
}

std::size_t CitySimulation::zoneable_tile_count(GridRect area, ZoneType zone_type) const {
  if (area.width <= 0 || area.height <= 0) {
    return 0;
  }

  std::size_t count = 0;
  const int end_x = area.x + area.width;
  const int end_y = area.y + area.height;
  for (int y = area.y; y < end_y; ++y) {
    for (int x = area.x; x < end_x; ++x) {
      const GridPoint point{x, y};
      if (!map_.in_bounds(point)) {
        continue;
      }
      const Tile& tile = map_.at(point);
      if (tile.terrain == TerrainType::Grass && !tile.road &&
          tile.building == k_invalid_building_id && tile.zone != zone_type) {
        ++count;
      }
    }
  }
  return count;
}

bool CitySimulation::can_afford(int cost) const {
  return cost >= 0 && treasury_ >= cost;
}

void CitySimulation::grow_one_building() {
  const DemandState demand = calculate_demand();
  for (std::size_t y = 0; y < map_.height(); ++y) {
    for (std::size_t x = 0; x < map_.width(); ++x) {
      const GridPoint point{static_cast<int>(x), static_cast<int>(y)};
      const Tile& tile = map_.at(point);
      if (!tile.is_buildable() || tile.zone == ZoneType::None ||
          !map_.has_adjacent_road(point) || !demand_allows_growth(demand, tile.zone)) {
        continue;
      }

      const BuildingId id = next_building_id_++;
      const Building building = building_for_zone(id, tile.zone, point);
      if (!map_.place_building(point, id)) {
        continue;
      }

      buildings_.push_back(building);
      return;
    }
  }
}

void CitySimulation::process_month() {
  int revenue = 0;
  int maintenance = static_cast<int>(road_tile_count()) * config_.road_maintenance_per_tile;

  for (const Building& building : buildings_) {
    maintenance += config_.building_maintenance;
    switch (building.type) {
      case BuildingType::House:
        revenue += building.residents * config_.residential_tax_per_person;
        break;
      case BuildingType::Shop:
        revenue += building.jobs * config_.commercial_tax_per_job;
        break;
      case BuildingType::Workshop:
        revenue += building.jobs * config_.industrial_tax_per_job;
        break;
    }
  }

  last_month_balance_ = revenue - maintenance;
  treasury_ += last_month_balance_;
}

void CitySimulation::refresh_cached_stats() {
  cached_stats_.tick = tick_;
  cached_stats_.treasury = treasury_;
  cached_stats_.population = population();
  cached_stats_.jobs = jobs();
  cached_stats_.unemployment = std::max(0, cached_stats_.population - cached_stats_.jobs);
  cached_stats_.last_month_balance = last_month_balance_;
  cached_stats_.road_tiles = road_tile_count();
  cached_stats_.building_count = buildings_.size();
  cached_stats_.demand = calculate_demand();
}

void CitySimulation::remove_building_by_id(BuildingId id) {
  buildings_.erase(
      std::remove_if(buildings_.begin(), buildings_.end(), [id](const Building& building) {
        return building.id == id;
      }),
      buildings_.end());
}

}  // namespace romulus::sim
