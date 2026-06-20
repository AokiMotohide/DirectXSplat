#include "common/Bounding.h"

#include <algorithm>
#include <cmath>

namespace dxsplat {

Aabb ComputeAabb(const std::vector<Vec3>& points) {
  Aabb out{};
  if (points.empty()) {
    return out;
  }
  out.min = points[0];
  out.max = points[0];
  out.valid = true;
  for (const auto& p : points) {
    out.min = Min(out.min, p);
    out.max = Max(out.max, p);
  }
  return out;
}

Aabb ComputePercentileAabb(const std::vector<Vec3>& points, float lowPercentile, float highPercentile) {
  Aabb out{};
  if (points.empty()) {
    return out;
  }
  std::vector<float> xs;
  std::vector<float> ys;
  std::vector<float> zs;
  xs.reserve(points.size());
  ys.reserve(points.size());
  zs.reserve(points.size());
  for (const auto& p : points) {
    xs.push_back(p.x);
    ys.push_back(p.y);
    zs.push_back(p.z);
  }

  auto getPercentile = [](std::vector<float>& values, float p) {
    std::sort(values.begin(), values.end());
    const float t = Clamp(p, 0.0f, 100.0f) * 0.01f;
    const size_t idx = static_cast<size_t>(std::round((values.size() - 1) * t));
    return values[std::min(idx, values.size() - 1)];
  };

  out.min = {getPercentile(xs, lowPercentile), getPercentile(ys, lowPercentile), getPercentile(zs, lowPercentile)};
  out.max = {getPercentile(xs, highPercentile), getPercentile(ys, highPercentile), getPercentile(zs, highPercentile)};
  out.valid = true;
  return out;
}

Vec3 ComputeAabbCenter(const Aabb& aabb) {
  return {(aabb.min.x + aabb.max.x) * 0.5f, (aabb.min.y + aabb.max.y) * 0.5f,
          (aabb.min.z + aabb.max.z) * 0.5f};
}

float ComputeAabbRadius(const Aabb& aabb) {
  const Vec3 ext = aabb.max - aabb.min;
  return Length(ext) * 0.5f;
}

}  // namespace dxsplat