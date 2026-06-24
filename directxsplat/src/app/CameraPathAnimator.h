#pragma once

#include <cstddef>
#include <vector>

#include "app/CameraController.h"
#include "dxsplat/directxsplat.h"

namespace dxsplat {

struct AnimationUiState {
  bool enabled = false;
  float fps = 1.0f;
  float time = 0.0f;
};

void ClampAnimationUiState(AnimationUiState& state, size_t cameraCount);
void StopAnimationOnCameraEdit(AnimationUiState& state, bool cameraEdited);

class CameraPathAnimator {
 public:
  void SetCameras(const CameraSet& cameras);
  void SetTime(float time);
  float Time() const;
  void Advance(float dt, float fps);
  bool Evaluate(CameraState& outState) const;

 private:
  struct Pose {
    Vec3 position{};
    Quat orientation{};
  };

  static CameraState CameraStateFromPose(const Pose& pose);

  std::vector<Pose> poses_;
  float time_ = 0.0f;
};

}
