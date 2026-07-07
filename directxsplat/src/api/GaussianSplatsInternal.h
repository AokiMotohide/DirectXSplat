#pragma once

#include <memory>

#include "directxsplat/directxsplat.h"
#include "directxsplat/scene.h"

namespace directxsplat {

class GaussianSplats::Impl {
 public:
  Impl();
  explicit Impl(Scene scene);

  Scene scene;
  uint64_t size = 0;
  Aabb bounds;
};

namespace internal {

class GaussianSplatsStorage {
 public:
  static std::shared_ptr<GaussianSplats::Impl>& ImplOf(GaussianSplats& splats);
  static const std::shared_ptr<GaussianSplats::Impl>& ImplOf(const GaussianSplats& splats);
  static GaussianSplats Make(std::shared_ptr<GaussianSplats::Impl> impl);
  static GaussianSplats Make(Scene scene);
  static std::shared_ptr<GaussianSplats::Impl>& EnsureImpl(GaussianSplats& splats);
};

}  // namespace internal

GaussianSplats MakeGaussianSplats(Scene scene);
const Aabb& BoundsFromSplats(const GaussianSplats& splats);
Scene& SceneFromSplats(GaussianSplats& splats);
const Scene& SceneFromSplats(const GaussianSplats& splats);

}  // namespace directxsplat
