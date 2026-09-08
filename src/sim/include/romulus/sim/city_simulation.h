#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "romulus/sim/city_map.h"

namespace romulus::sim {

struct SimulationConfig {
  int initial_treasury = 10'000;
  int road_build_cost = 10;
  int zoning_cost_per_tile = 1;
  int bulldoze_cost = 2;
  int road_maintenance_per_tile = 1;
  int building_maintenance = 1;
  int residential_tax_per_person = 2;
  int commercial_tax_per_job = 2;
  int industrial_tax_per_job = 1;
  std::uint64_t growth_interval_ticks = 8;
  std::uint64_t economy_interval_ticks = 30;
};

struct DemandState {
  int residential = 0;
  int commercial = 0;
  int industrial = 0;
};

struct CityStats {
  std::uint64_t tick = 0;
  int treasury = 0;
  int population = 0;
  int jobs = 0;
  int unemployment = 0;
  int last_month_balance = 0;
  std::size_t road_tiles = 0;
  std::size_t building_count = 0;
  DemandState demand{};
};

class CitySimulation {
 public:
  CitySimulation(std::size_t width, std::size_t height, SimulationConfig config = {});

  [[nodiscard]] const CityMap& map() const;
  [[nodiscard]] const std::vector<Building>& buildings() const;
  [[nodiscard]] const Building* building_at(GridPoint point) const;
  [[nodiscard]] CityStats stats() const;
  [[nodiscard]] std::uint64_t state_hash() const;

  bool set_terrain(GridPoint point, TerrainType terrain);
  bool build_road(GridPoint point);
  std::size_t zone(GridRect area, ZoneType zone);
  bool bulldoze(GridPoint point);

  void advance_tick();
  void advance_ticks(std::size_t count);

 private:
  [[nodiscard]] DemandState calculate_demand() const;
  [[nodiscard]] int population() const;
  [[nodiscard]] int jobs() const;
  [[nodiscard]] std::size_t road_tile_count() const;
  [[nodiscard]] std::size_t zoneable_tile_count(GridRect area, ZoneType zone) const;
  [[nodiscard]] bool can_afford(int cost) const;

  void grow_one_building();
  void process_month();
  void refresh_cached_stats();
  void remove_building_by_id(BuildingId id);

  CityMap map_;
  SimulationConfig config_;
  std::vector<Building> buildings_;
  BuildingId next_building_id_ = 1;
  std::uint64_t tick_ = 0;
  int treasury_ = 0;
  int last_month_balance_ = 0;
  CityStats cached_stats_{};
};

}  // namespace romulus::sim
