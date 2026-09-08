#include "romulus/sim/city_simulation.h"

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

void build_test_city(romulus::sim::CitySimulation& simulation) {
  using namespace romulus::sim;
  for (int x = 0; x < 8; ++x) {
    simulation.build_road({x, 3});
  }
  simulation.zone({0, 2, 3, 1}, ZoneType::Residential);
  simulation.zone({3, 2, 2, 1}, ZoneType::Commercial);
  simulation.zone({5, 2, 3, 1}, ZoneType::Industrial);
}

}  // namespace

int main() {
  using namespace romulus::sim;

  SimulationConfig config;
  config.initial_treasury = 1'000;
  config.growth_interval_ticks = 1;
  config.economy_interval_ticks = 5;

  CitySimulation simulation(8, 6, config);
  build_test_city(simulation);

  const int treasury_after_construction = simulation.stats().treasury;
  if (!expect(treasury_after_construction == 912, "road and zoning costs should be charged deterministically")) {
    return EXIT_FAILURE;
  }

  simulation.advance_ticks(8);
  const CityStats stats = simulation.stats();
  if (!expect(stats.population > 0, "residential zoning with road access should grow houses")) {
    return EXIT_FAILURE;
  }
  if (!expect(stats.jobs > 0, "commercial or industrial zoning should create jobs after population appears")) {
    return EXIT_FAILURE;
  }
  if (!expect(stats.building_count > 1, "multiple zoned tiles should develop over time")) {
    return EXIT_FAILURE;
  }
  if (!expect(stats.last_month_balance != 0, "monthly economy should compute a non-zero balance")) {
    return EXIT_FAILURE;
  }

  CitySimulation replay(8, 6, config);
  build_test_city(replay);
  replay.advance_ticks(8);
  if (!expect(replay.state_hash() == simulation.state_hash(), "identical inputs should produce identical simulation state")) {
    return EXIT_FAILURE;
  }

  const Building* building = simulation.building_at({0, 2});
  if (!expect(building != nullptr, "first residential tile should have developed deterministically")) {
    return EXIT_FAILURE;
  }
  if (!expect(simulation.bulldoze({0, 2}), "developed building should be bulldozable")) {
    return EXIT_FAILURE;
  }
  if (!expect(simulation.building_at({0, 2}) == nullptr, "bulldozing should remove the building record")) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
