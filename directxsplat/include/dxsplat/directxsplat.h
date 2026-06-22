#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "dxsplat/status.h"

namespace dxsplat {

enum class RenderType {
  Color,
  Alpha,
  Depth,
};

enum class ShadingDegree {
  Dc = 0,
  Degree1 = 1,
  Degree2 = 2,
  Degree3 = 3,
};

struct CameraParams {
  std::array<float, 16> extrinsic{};
  std::array<float, 9> intrinsic{};
  uint32_t width = 0;
  uint32_t height = 0;
  std::string name;
};

struct CameraSet {
  std::vector<CameraParams> cameras;
};

struct DrawOptions {
  uint32_t width = 1600;
  uint32_t height = 900;
  float nearPlane = 0.1f;
  float farPlane = 5000.0f;
  float background[3] = {0.0f, 0.0f, 0.0f};
  bool antialiasing = true;
  float antialiasingStrength = 1.0f;
  bool gammaCorrection = false;
  RenderType renderType = RenderType::Color;
  ShadingDegree shadingDegree = ShadingDegree::Degree3;
};

struct ImageRgba8 {
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint8_t> pixels;
};

class GaussianSplats {
 public:
  GaussianSplats();
  ~GaussianSplats();
  GaussianSplats(GaussianSplats&&) noexcept;
  GaussianSplats& operator=(GaussianSplats&&) noexcept;
  GaussianSplats(const GaussianSplats&) = delete;
  GaussianSplats& operator=(const GaussianSplats&) = delete;

  uint64_t Size() const;
  bool Empty() const;

 private:
  class Impl;
  friend class GaussianSplatsAccess;

  explicit GaussianSplats(std::shared_ptr<Impl> impl);

  std::shared_ptr<Impl> impl_;
};

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
