#pragma once

#include <memory>

#include "dxsplat/directxsplat.h"
#include "dxsplat/scene.h"

namespace dxsplat {

class GaussianSplats::Impl {
 public:
  Impl();
  explicit Impl(Scene scene);

  Scene scene;
  uint64_t size = 0;
  Aabb bounds;
};

class GaussianSplatsAccess {
 public:
  static std::shared_ptr<GaussianSplats::Impl>& ImplOf(GaussianSplats& splats);
  static const std::shared_ptr<GaussianSplats::Impl>& ImplOf(const GaussianSplats& splats);
  static GaussianSplats Make(std::shared_ptr<GaussianSplats::Impl> impl);
  static GaussianSplats Make(Scene scene);
  static std::shared_ptr<GaussianSplats::Impl>& EnsureImpl(GaussianSplats& splats);
};

GaussianSplats MakeGaussianSplats(Scene scene);
const Aabb& BoundsFromSplats(const GaussianSplats& splats);
Scene& SceneFromSplats(GaussianSplats& splats);
const Scene& SceneFromSplats(const GaussianSplats& splats);

}  // namespace dxsplat
