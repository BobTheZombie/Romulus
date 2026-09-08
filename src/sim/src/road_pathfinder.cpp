#include "romulus/sim/road_pathfinder.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <queue>
#include <vector>

namespace romulus::sim {
namespace {

int manhattan_distance(GridPoint lhs, GridPoint rhs) {
  return std::abs(lhs.x - rhs.x) + std::abs(lhs.y - rhs.y);
}

std::size_t point_index(const CityMap& map, GridPoint point) {
  return static_cast<std::size_t>(point.y) * map.width() + static_cast<std::size_t>(point.x);
}

GridPoint index_point(const CityMap& map, std::size_t index) {
  return GridPoint{
      static_cast<int>(index % map.width()),
      static_cast<int>(index / map.width()),
  };
}

struct OpenNode {
  GridPoint point{};
  int g = 0;
  int h = 0;

  [[nodiscard]] int f() const {
    return g + h;
  }
};

struct OpenNodeGreater {
  bool operator()(const OpenNode& lhs, const OpenNode& rhs) const {
    if (lhs.f() != rhs.f()) {
      return lhs.f() > rhs.f();
    }
    if (lhs.h != rhs.h) {
      return lhs.h > rhs.h;
    }
    if (lhs.point.y != rhs.point.y) {
      return lhs.point.y > rhs.point.y;
    }
    return lhs.point.x > rhs.point.x;
  }
};

}  // namespace

std::vector<GridPoint> find_road_path(const CityMap& map, GridPoint start, GridPoint goal) {
  if (!map.in_bounds(start) || !map.in_bounds(goal) || !map.at(start).road || !map.at(goal).road) {
    return {};
  }

  if (start == goal) {
    return {start};
  }

  const std::size_t tile_count = map.width() * map.height();
  const int k_unreachable = std::numeric_limits<int>::max();
  const std::size_t k_no_parent = tile_count;

  std::vector<int> g_score(tile_count, k_unreachable);
  std::vector<std::size_t> parent(tile_count, k_no_parent);
  std::priority_queue<OpenNode, std::vector<OpenNode>, OpenNodeGreater> open;

  const std::size_t start_index = point_index(map, start);
  const std::size_t goal_index = point_index(map, goal);
  g_score[start_index] = 0;
  open.push(OpenNode{start, 0, manhattan_distance(start, goal)});

  while (!open.empty()) {
    const OpenNode current = open.top();
    open.pop();

    const std::size_t current_index = point_index(map, current.point);
    if (current.g != g_score[current_index]) {
      continue;
    }

    if (current_index == goal_index) {
      break;
    }

    for (const GridPoint neighbor : map.neighbors4(current.point)) {
      if (!map.at(neighbor).road) {
        continue;
      }

      const std::size_t neighbor_index = point_index(map, neighbor);
      const int candidate_g = current.g + 1;
      if (candidate_g >= g_score[neighbor_index]) {
        continue;
      }

      g_score[neighbor_index] = candidate_g;
      parent[neighbor_index] = current_index;
      open.push(OpenNode{neighbor, candidate_g, manhattan_distance(neighbor, goal)});
    }
  }

  if (g_score[goal_index] == k_unreachable) {
    return {};
  }

  std::vector<GridPoint> path;
  for (std::size_t index = goal_index;; index = parent[index]) {
    path.push_back(index_point(map, index));
    if (index == start_index) {
      break;
    }
    if (parent[index] == k_no_parent) {
      return {};
    }
  }

  std::reverse(path.begin(), path.end());
  return path;
}

std::optional<std::size_t> road_distance(const CityMap& map, GridPoint start, GridPoint goal) {
  const std::vector<GridPoint> path = find_road_path(map, start, goal);
  if (path.empty()) {
    return std::nullopt;
  }
  return path.size() - 1;
}

}  // namespace romulus::sim
