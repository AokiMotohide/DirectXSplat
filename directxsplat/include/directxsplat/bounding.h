#pragma once

#include <vector>

#include "directxsplat/math.h"

namespace directxsplat {

struct Aabb {
  Vec3 min{0.0f, 0.0f, 0.0f};
  Vec3 max{0.0f, 0.0f, 0.0f};
  bool valid = false;
};

Aabb ComputeAabb(const std::vector<Vec3>& points);
Aabb ComputePercentileAabb(const std::vector<Vec3>& points, float lowPercentile, float highPercentile);
Vec3 ComputeAabbCenter(const Aabb& aabb);
float ComputeAabbRadius(const Aabb& aabb);

}  // namespace directxsplat
