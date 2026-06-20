#include "dxsplat/directxsplat.h"

#include <memory>

#include "api/ViewerInternal.h"

namespace dxsplat {

Status Viewer::Impl::Initialize(const ViewerConfig& config) {
  if (config.width == 0) {
    return Status::Error("viewer width must be greater than zero");
  }
  if (config.height == 0) {
    return Status::Error("viewer height must be greater than zero");
  }

  Status status = application_.Initialize(config);
  initialized_ = status.ok;
  return status;
}

Status Viewer::Impl::Load(const std::filesystem::path& scenePath) {
  if (!initialized_) {
    return Status::Error("viewer is not initialized");
  }
  return application_.Load(scenePath);
}

Status Viewer::Impl::Run() {
  if (!initialized_) {
    return Status::Error("viewer is not initialized");
  }
  return application_.Run();
}

void Viewer::Impl::RequestClose() {
  if (initialized_) {
    application_.RequestClose();
  }
}

Viewer::Viewer() : impl_(std::make_unique<Impl>()) {}

Viewer::~Viewer() = default;

Viewer::Viewer(Viewer&&) noexcept = default;

Viewer& Viewer::operator=(Viewer&&) noexcept = default;

Status Viewer::Initialize(const ViewerConfig& config) {
  if (!impl_) {
    impl_ = std::make_unique<Impl>();
  }
  return impl_->Initialize(config);
}

Status Viewer::Load(const std::filesystem::path& scenePath) {
  if (!impl_) {
    return Status::Error("viewer is not initialized");
  }
  return impl_->Load(scenePath);
}

Status Viewer::Run() {
  if (!impl_) {
    return Status::Error("viewer is not initialized");
  }
  return impl_->Run();
}

void Viewer::RequestClose() {
  if (impl_) {
    impl_->RequestClose();
  }
}

Status Show(const ViewerConfig& config) {
  Viewer viewer;
  Status status = viewer.Initialize(config);
  if (!status.ok) {
    return status;
  }
  return viewer.Run();
}

Status Show(const std::filesystem::path& scenePath) {
  ViewerConfig config{};
  config.initialScenePath = scenePath;
  return Show(config);
}

}  // namespace dxsplat
