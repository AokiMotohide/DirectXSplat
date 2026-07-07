#include "api/GaussianSplatsInternal.h"

#include <memory>
#include <utility>

namespace directxsplat {

namespace {

uint64_t CountSplats(const Scene& scene) {
  uint64_t out = 0;
  for (const auto& set : scene.splatSets) {
    out += static_cast<uint64_t>(set.gaussians.size());
  }
  return out;
}

Aabb MergeBounds(const Scene& scene) {
  if (scene.sceneBounds.valid) {
    return scene.sceneBounds;
  }

  Aabb out{};
  for (const auto& set : scene.splatSets) {
    if (!set.bounds.valid) {
      continue;
    }
    if (!out.valid) {
      out = set.bounds;
      continue;
    }
    out.min = Min(out.min, set.bounds.min);
    out.max = Max(out.max, set.bounds.max);
  }
  return out;
}

}  // namespace

GaussianSplats::Impl::Impl() = default;

GaussianSplats::Impl::Impl(Scene sceneIn)
    : scene(std::move(sceneIn)), size(CountSplats(scene)), bounds(MergeBounds(scene)) {}

std::shared_ptr<GaussianSplats::Impl>& internal::GaussianSplatsStorage::ImplOf(GaussianSplats& splats) {
  return splats.impl_;
}

const std::shared_ptr<GaussianSplats::Impl>& internal::GaussianSplatsStorage::ImplOf(const GaussianSplats& splats) {
  return splats.impl_;
}

GaussianSplats internal::GaussianSplatsStorage::Make(std::shared_ptr<GaussianSplats::Impl> impl) {
  return GaussianSplats(std::move(impl));
}

GaussianSplats internal::GaussianSplatsStorage::Make(Scene scene) {
  return GaussianSplats(std::make_shared<GaussianSplats::Impl>(std::move(scene)));
}

std::shared_ptr<GaussianSplats::Impl>& internal::GaussianSplatsStorage::EnsureImpl(GaussianSplats& splats) {
  auto& impl = splats.impl_;
  if (!impl) {
    impl = std::make_shared<GaussianSplats::Impl>();
  }
  return impl;
}

GaussianSplats::GaussianSplats() : impl_(std::make_shared<Impl>()) {}

GaussianSplats::GaussianSplats(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
}

GaussianSplats::~GaussianSplats() = default;
GaussianSplats::GaussianSplats(GaussianSplats&&) noexcept = default;
GaussianSplats& GaussianSplats::operator=(GaussianSplats&&) noexcept = default;

uint64_t GaussianSplats::Size() const {
  return impl_ ? impl_->size : 0;
}

bool GaussianSplats::Empty() const {
  return Size() == 0;
}

GaussianSplats MakeGaussianSplats(Scene scene) {
  return internal::GaussianSplatsStorage::Make(std::move(scene));
}

const Aabb& BoundsFromSplats(const GaussianSplats& splats) {
  static const Aabb emptyBounds{};
  const auto& impl = internal::GaussianSplatsStorage::ImplOf(splats);
  return impl ? impl->bounds : emptyBounds;
}

Scene& SceneFromSplats(GaussianSplats& splats) {
  return internal::GaussianSplatsStorage::EnsureImpl(splats)->scene;
}

const Scene& SceneFromSplats(const GaussianSplats& splats) {
  static const Scene emptyScene{};
  const auto& impl = internal::GaussianSplatsStorage::ImplOf(splats);
  return impl ? impl->scene : emptyScene;
}

}  // namespace directxsplat
