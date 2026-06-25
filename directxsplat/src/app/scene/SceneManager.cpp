#include "scene/SceneManager.h"

namespace directxsplat {

Status SceneManager::AddScene(Scene scene, UploadedSceneHandle uploadedScene, std::vector<UploadedChunkHandle> uploadedChunks) {
  scenes_.push_back(SceneEntry{std::move(scene), uploadedScene, std::move(uploadedChunks)});
  activeScene_ = scenes_.empty() ? 0 : scenes_.size() - 1;
  return Status::Ok();
}

void SceneManager::Clear() {
  scenes_.clear();
  activeScene_ = 0;
}

const std::vector<SceneEntry>& SceneManager::Scenes() const { return scenes_; }

std::vector<SceneEntry>& SceneManager::Scenes() { return scenes_; }

size_t SceneManager::ActiveSceneIndex() const { return activeScene_; }

Status SceneManager::SetActiveSceneIndex(size_t index) {
  if (index >= scenes_.size()) {
    return Status::Error("scene index out of range");
  }
  activeScene_ = index;
  return Status::Ok();
}

Scene* SceneManager::ActiveScene() {
  if (scenes_.empty()) {
    return nullptr;
  }
  return &scenes_[activeScene_].scene;
}

const Scene* SceneManager::ActiveScene() const {
  if (scenes_.empty()) {
    return nullptr;
  }
  return &scenes_[activeScene_].scene;
}

UploadedSceneHandle SceneManager::ActiveUploadedScene() const {
  if (scenes_.empty()) {
    return {};
  }
  return scenes_[activeScene_].uploadedScene;
}

}  // namespace directxsplat
