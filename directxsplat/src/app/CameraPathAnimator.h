#pragma once

#include <cstddef>
#include <vector>

#include "app/CameraController.h"
#include "dxsplat/directxsplat.h"

namespace directxsplat {

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

  struct PoseTangent {
    Vec3 rotation{};
    Vec3 translation{};
  };

  static CameraState CameraStateFromPose(const Pose& pose);
  static Pose ExpPose(const PoseTangent& tangent);
  static PoseTangent LogPose(const Pose& pose);
  static Pose InversePose(const Pose& pose);
  static Pose ComposePose(const Pose& a, const Pose& b);
  static Pose InterpolatePose(const Pose& p0, const Pose& p1, const Pose& p2, const Pose& p3, float t);

  std::vector<Pose> poses_;
  float time_ = 0.0f;
};

}
