#include "dxsplat/directxsplat.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

#include "api/GaussianSplatsInternal.h"
#include "dxsplat/io.h"

namespace dxsplat {

namespace {

std::string LowerExtension(const std::filesystem::path& path) {
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return extension;
}

StatusOr<GaussianSplats> LoadExpectedExtension(const std::filesystem::path& scenePath,
                                               const char* extension,
                                               const char* message) {
  if (scenePath.empty()) {
    return StatusOr<GaussianSplats>::Error("scene path is empty");
  }
  if (LowerExtension(scenePath) != extension) {
    return StatusOr<GaussianSplats>::Error(message);
  }
  return LoadFromFile(scenePath);
}

}  // namespace

StatusOr<GaussianSplats> LoadFromFile(const std::filesystem::path& scenePath) {
  if (scenePath.empty()) {
    return StatusOr<GaussianSplats>::Error("scene path is empty");
  }

  auto scene = LoadSceneFromFile(scenePath.string());
  if (!scene.ok()) {
    return StatusOr<GaussianSplats>::Error(scene.status.message);
  }
  return StatusOr<GaussianSplats>::Ok(MakeGaussianSplats(std::move(scene.value)));
}

StatusOr<GaussianSplats> LoadFromPly(const std::filesystem::path& scenePath) {
  return LoadExpectedExtension(scenePath, ".ply", "expected .ply scene path");
}

StatusOr<GaussianSplats> LoadFromSpz(const std::filesystem::path& scenePath) {
  return LoadExpectedExtension(scenePath, ".spz", "expected .spz scene path");
}

}  // namespace dxsplat
