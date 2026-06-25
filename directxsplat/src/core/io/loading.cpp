#include "dxsplat/io.h"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

#include "filesystem.hpp"
#include "io/common/string_util.h"
#include "io/format_detection.h"
#include "io/scene_loader.h"

namespace directxsplat {

namespace fs = ghc::filesystem;

namespace {

constexpr size_t kMaxBackgroundScenePaths = 65536;
constexpr size_t kMaxPendingSceneLoads = 256;

}

SceneFormat DetectSceneFormat(const std::string& path) { return io::DetectSceneFormat(path); }

StatusOr<Scene> LoadSceneFromFile(const std::string& path, const SceneLoadOptions& options) {
  io::SceneLoader loader;
  auto result = loader.Load(path, options);
  if (!result.ok()) {
    return StatusOr<Scene>::Error(result.status.message);
  }
  return StatusOr<Scene>::Ok(std::move(result.value));
}

BackgroundSceneLoader::BackgroundSceneLoader() = default;

BackgroundSceneLoader::~BackgroundSceneLoader() { Shutdown(); }

Status BackgroundSceneLoader::Initialize(const std::string& folderPath, SceneLoadOptions options) try {
  Shutdown();

  const fs::path folder(folderPath);
  if (!fs::exists(folder) || !fs::is_directory(folder)) {
    return Status::Error("scene traversal path is not a directory");
  }

  std::vector<std::string> scenePaths;
  auto appendScenePath = [&](const fs::path& path) -> Status {
    if (scenePaths.size() >= kMaxBackgroundScenePaths) {
      return Status::Error("too many scene paths");
    }
    scenePaths.push_back(path.string());
    return Status::Ok();
  };
  for (const auto& entry : fs::directory_iterator(folder)) {
    if (entry.is_directory()) {
      if (DetectSceneFormat(entry.path().string()) != SceneFormat::Unknown) {
        Status append = appendScenePath(entry.path());
        if (!append.ok) {
          return append;
        }
      }
      continue;
    }
    if (!entry.is_regular_file()) {
      continue;
    }
    const SceneFormat format = DetectSceneFormat(entry.path().string());
    if (format != SceneFormat::Unknown) {
      Status append = appendScenePath(entry.path());
      if (!append.ok) {
        return append;
      }
    }
  }

  std::sort(scenePaths.begin(), scenePaths.end());
  {
    std::lock_guard<std::mutex> lock(mutex_);
    scenePaths_ = std::move(scenePaths);
    options_ = std::move(options);
    pending_.clear();
    completed_.clear();
    running_ = true;
    worker_ = std::thread([this]() { WorkerMain(); });
  }
  return Status::Ok();
} catch (const fs::filesystem_error&) {
  Shutdown();
  return Status::Error("scene traversal filesystem error");
} catch (const std::bad_alloc&) {
  Shutdown();
  return Status::Error("scene traversal allocation failed");
} catch (const std::length_error&) {
  Shutdown();
  return Status::Error("scene traversal allocation failed");
} catch (const std::exception&) {
  Shutdown();
  return Status::Error("scene traversal initialization failed");
}

void BackgroundSceneLoader::Shutdown() {
  std::thread worker;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    pending_.clear();
    completed_.clear();
    if (worker_.joinable()) {
      worker = std::move(worker_);
    }
  }
  cv_.notify_all();
  if (worker.joinable()) {
    worker.join();
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.clear();
    completed_.clear();
  }
}

void BackgroundSceneLoader::RequestLoad(size_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!running_ || index >= scenePaths_.size()) {
    return;
  }
  auto enqueue = [&](size_t value) {
    try {
      if (std::find(pending_.begin(), pending_.end(), value) != pending_.end()) {
        return;
      }
      while (pending_.size() >= kMaxPendingSceneLoads) {
        pending_.pop_front();
      }
      pending_.push_back(value);
    } catch (const std::bad_alloc&) {
      pending_.clear();
    } catch (const std::length_error&) {
      pending_.clear();
    }
  };
  enqueue(index);
  if (index + 1 < scenePaths_.size()) {
    enqueue(index + 1);
  }
  if (index > 0) {
    enqueue(index - 1);
  }
  cv_.notify_one();
}

bool BackgroundSceneLoader::PollLoaded(size_t& index, Scene& scene, std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (completed_.empty()) {
    return false;
  }
  CompletedItem item = std::move(completed_.front());
  completed_.pop_front();
  index = item.index;
  error = std::move(item.error);
  if (item.scene.has_value()) {
    scene = std::move(*item.scene);
  }
  return true;
}

size_t BackgroundSceneLoader::SceneCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scenePaths_.size();
}

std::vector<std::string> BackgroundSceneLoader::ScenePaths() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scenePaths_;
}

void BackgroundSceneLoader::WorkerMain() {
  auto makeErrorItem = [](size_t itemIndex, const char* message) noexcept {
    CompletedItem item{};
    item.index = itemIndex;
    try {
      item.error = message;
    } catch (...) {
      try {
        item.error = "error";
      } catch (...) {
      }
    }
    return item;
  };

  while (true) {
    size_t index = std::numeric_limits<size_t>::max();
    std::string scenePath;
    SceneLoadOptions options;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this]() { return !running_ || !pending_.empty(); });
      if (!running_) {
        break;
      }
      index = pending_.front();
      pending_.pop_front();
      pending_.erase(std::remove(pending_.begin(), pending_.end(), index), pending_.end());
      if (index >= scenePaths_.size()) {
        continue;
      }
      scenePath = scenePaths_[index];
      options = options_;
    }

    CompletedItem item{};
    try {
      item.index = index;
      const auto result = LoadSceneFromFile(scenePath, options);
      if (result.ok()) {
        item.scene = std::move(result.value);
      } else {
        item.error = result.status.message;
      }
    } catch (const std::bad_alloc&) {
      item = makeErrorItem(index, "scene traversal allocation failed");
    } catch (const std::length_error&) {
      item = makeErrorItem(index, "scene traversal allocation failed");
    } catch (const std::exception&) {
      item = makeErrorItem(index, "scene traversal load failed");
    } catch (...) {
      item = makeErrorItem(index, "scene traversal load failed");
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!running_) {
        break;
      }
      while (completed_.size() >= 8) {
        completed_.pop_front();
      }
      auto publishFailure = [&]() noexcept {
        try {
          completed_.clear();
          CompletedItem errorItem = makeErrorItem(index, "scene traversal allocation failed");
          completed_.push_back(std::move(errorItem));
        } catch (...) {
          completed_.clear();
        }
      };
      try {
        completed_.push_back(std::move(item));
      } catch (const std::bad_alloc&) {
        publishFailure();
      } catch (const std::length_error&) {
        publishFailure();
      }
    }
  }
}

}
