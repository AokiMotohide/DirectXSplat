#pragma once

#include <filesystem>

#include "app/Application.h"
#include "dxsplat/directxsplat.h"

namespace dxsplat {

class Viewer::Impl {
 public:
  Status Initialize(const ViewerConfig& config);
  Status Load(const std::filesystem::path& scenePath);
  Status Run();
  void RequestClose();

 private:
  Application application_;
  bool initialized_ = false;
};

}  // namespace dxsplat
