#pragma once

#include <memory>
#include <string>
#include <vector>

#include "directxsplat/renderer.h"
#include "directxsplat/scene.h"
#include "directxsplat/status.h"

namespace directxsplat {

struct SceneEntry {
  Scene scene{};
  UploadedSceneHandle uploadedScene{};
  std::vector<UploadedChunkHandle> uploadedChunks;
};

class SceneManager {
 public:
  Status AddScene(Scene scene, UploadedSceneHandle uploadedScene, std::vector<UploadedChunkHandle> uploadedChunks = {});
  void Clear();

  const std::vector<SceneEntry>& Scenes() const;
  std::vector<SceneEntry>& Scenes();

  size_t ActiveSceneIndex() const;
  Status SetActiveSceneIndex(size_t index);
  Scene* ActiveScene();
  const Scene* ActiveScene() const;
  UploadedSceneHandle ActiveUploadedScene() const;

 private:
  std::vector<SceneEntry> scenes_;
  size_t activeScene_ = 0;
};

}  // namespace directxsplat
