#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

#include "dxsplat/status.h"

namespace dxsplat {

struct ViewerConfig {
  std::filesystem::path initialScenePath;
  std::filesystem::path sceneFolderPath;
  std::filesystem::path sourceImageDirectory;
  uint32_t width = 1600;
  uint32_t height = 900;
  bool vsync = false;
};

class Viewer {
 public:
  Viewer();
  ~Viewer();
  Viewer(Viewer&&) noexcept;
  Viewer& operator=(Viewer&&) noexcept;
  Viewer(const Viewer&) = delete;
  Viewer& operator=(const Viewer&) = delete;

  Status Initialize(const ViewerConfig& config = {});
  Status Load(const std::filesystem::path& scenePath);
  Status Run();
  void RequestClose();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

Status Show(const ViewerConfig& config = {});
Status Show(const std::filesystem::path& scenePath);

}  // namespace dxsplat
