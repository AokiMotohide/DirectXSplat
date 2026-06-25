#pragma once

#include <cstdint>

#include "dxsplat/bounding.h"
#include "dxsplat/math.h"
#include "dxsplat/status.h"

struct ID3D12Fence;

namespace directxsplat {

struct UploadSyncPoint {
  ID3D12Fence* fence = nullptr;
  uint64_t value = 0;
  constexpr bool IsValid() const { return fence != nullptr && value != 0; }
};

struct UploadedSceneHandle {
  uint64_t value = 0;
  constexpr bool IsValid() const { return value != 0; }
  friend constexpr bool operator==(UploadedSceneHandle a, UploadedSceneHandle b) { return a.value == b.value; }
  friend constexpr bool operator!=(UploadedSceneHandle a, UploadedSceneHandle b) { return a.value != b.value; }
};

struct UploadedChunkHandle {
  uint64_t value = 0;
  constexpr bool IsValid() const { return value != 0; }
  friend constexpr bool operator==(UploadedChunkHandle a, UploadedChunkHandle b) { return a.value == b.value; }
  friend constexpr bool operator!=(UploadedChunkHandle a, UploadedChunkHandle b) { return a.value != b.value; }
};

struct SceneMutationToken {
  uint64_t value = 0;
  UploadedSceneHandle scene{};
  constexpr bool IsValid() const { return value != 0 && scene.IsValid(); }
  friend constexpr bool operator==(SceneMutationToken a, SceneMutationToken b) {
    return a.value == b.value && a.scene == b.scene;
  }
  friend constexpr bool operator!=(SceneMutationToken a, SceneMutationToken b) { return !(a == b); }
};

struct SceneAccessInfo {
  bool readyToRender = false;
  bool mutationActive = false;
  bool renderEncodingActive = false;
  uint32_t activeRenderEncoders = 0;
  uint32_t waitingMutations = 0;
};

}  // namespace directxsplat
