#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "dxsplat/scene.h"
#include "dxsplat/status.h"

namespace dxsplat {

enum class SceneFormat {
  Unknown,
  Ply,
  CompressedPly,
  Sog,
  HierarchicalLod,
  Spz,
  Splat,
};

struct SceneLoadOptions {
  std::string sourceImageDirectory;
};

SceneFormat DetectSceneFormat(const std::string& path);
StatusOr<Scene> LoadSceneFromFile(const std::string& path, const SceneLoadOptions& options = {});

class BackgroundSceneLoader {
 public:
  BackgroundSceneLoader();
  ~BackgroundSceneLoader();

  Status Initialize(const std::string& folderPath, SceneLoadOptions options = {});
  void Shutdown();

  void RequestLoad(size_t index);
  bool PollLoaded(size_t& index, Scene& scene, std::string& error);

  size_t SceneCount() const;
  std::vector<std::string> ScenePaths() const;

 private:
  struct CompletedItem {
    size_t index = 0;
    std::optional<Scene> scene;
    std::string error;
  };

  void WorkerMain();

  SceneLoadOptions options_;
  std::vector<std::string> scenePaths_;

  std::atomic<bool> running_{false};
  std::thread worker_;

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<size_t> pending_;
  std::deque<CompletedItem> completed_;
};

}
