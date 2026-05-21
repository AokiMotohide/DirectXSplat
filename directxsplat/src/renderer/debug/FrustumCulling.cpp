#include "renderer/debug/FrustumCulling.h"

#include <algorithm>

namespace dxsplat {

namespace {

FrustumPlane NormalizePlane(const FrustumPlane& p) {
  const float len = Length(p.n);
  if (len <= 1e-6f) {
    return p;
  }
  return {p.n / len, p.d / len};
}

}  

FrustumPlanes BuildFrustumPlanes(const Mat4& m) {
  FrustumPlanes p{};

  p[0] = NormalizePlane({{m.m[12] + m.m[0], m.m[13] + m.m[1], m.m[14] + m.m[2]}, m.m[15] + m.m[3]});
  p[1] = NormalizePlane({{m.m[12] - m.m[0], m.m[13] - m.m[1], m.m[14] - m.m[2]}, m.m[15] - m.m[3]});
  p[2] = NormalizePlane({{m.m[12] + m.m[4], m.m[13] + m.m[5], m.m[14] + m.m[6]}, m.m[15] + m.m[7]});
  p[3] = NormalizePlane({{m.m[12] - m.m[4], m.m[13] - m.m[5], m.m[14] - m.m[6]}, m.m[15] - m.m[7]});
  p[4] = NormalizePlane({{m.m[8], m.m[9], m.m[10]}, m.m[11]});
  p[5] = NormalizePlane({{m.m[12] - m.m[8], m.m[13] - m.m[9], m.m[14] - m.m[10]}, m.m[15] - m.m[11]});

  return p;
}

bool SphereInFrustum(const FrustumPlanes& planes, const Vec3& center, float radius) {
  for (const auto& p : planes) {
    if (Dot(p.n, center) + p.d < -radius) {
      return false;
    }
  }
  return true;
}

void FrustumCull(const Scene& scene, const FrustumPlanes& planes, std::vector<uint32_t>& visibleGlobalIndices) {
  visibleGlobalIndices.clear();

  uint32_t index = 0;
  for (const auto& set : scene.splatSets) {
    for (const auto& g : set.gaussians) {
      const float radius =
          std::max(std::max(std::abs(g.scale.x), std::abs(g.scale.y)), std::max(std::abs(g.scale.z), 1e-4f));
      if (SphereInFrustum(planes, g.position, radius)) {
        visibleGlobalIndices.push_back(index);
      }
      ++index;
    }
  }
}

}  // namespace dxsplat
