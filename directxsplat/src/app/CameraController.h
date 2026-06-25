#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dxsplat/directxsplat.h"
#include "dxsplat/math.h"
#include "dxsplat/scene.h"

namespace directxsplat {

enum class NavigatorMode {
  Fps,
  Orbit,
  Interpolation,
  Trackball,
  None,
};

inline constexpr float kDefaultCameraFovYRadians = 1.0471975512f;

struct CameraState {
  Vec3 position{0.0f, 0.0f, -3.0f};
  float yaw = 0.0f;
  float pitch = 0.0f;
  float roll = 0.0f;
  float fovYRadians = kDefaultCameraFovYRadians;
  float nearPlane = 0.1f;
  float farPlane = 5000.0f;
  float movementSpeed = 2.5f;
  float rotationSpeed = 1.0f;
  bool useAcceleration = false;
  NavigatorMode navigatorMode = NavigatorMode::Fps;
  Vec3 orbitPivot{0.0f, 0.0f, 0.0f};
  float orbitDistance = 3.0f;
};

struct CameraMatrixOverride {
  Mat4 view{};
  Vec3 position{};
};

class CameraController {
 public:
  CameraController();

  void SetViewport(uint32_t width, uint32_t height);
  void SetState(const CameraState& state);
  const CameraState& State() const;
  bool HasMatrixOverride() const;

  void UpdateFps(float dt, bool moveForward, bool moveBackward, bool moveLeft, bool moveRight, bool moveUp,
                 bool moveDown, float lookDeltaX, float lookDeltaY, float rollDelta, bool rotationEnabled);
  void UpdateOrbit(float dt, float orbitDeltaX, float orbitDeltaY, float panDeltaX, float panDeltaY,
                   float wheelDelta);
  void UpdateTrackball(float dt, float orbitDeltaX, float orbitDeltaY, float panDeltaX, float panDeltaY,
                       float wheelDelta);

  void FocusBounds(const Aabb& bounds);
  void FocusPoint(const Vec3& point, float estimatedRadius);
  void SetOrbitPivot(const Vec3& pivot);

  void SnapToInputCamera(const InputCamera& camera);
  void SnapToCameraParams(const CameraParams& camera);
  bool SnapToClosestInputCamera(const std::vector<InputCamera>& cameras);

  Mat4 ViewMatrix() const;
  Mat4 ProjectionMatrix() const;
  Mat4 ProjectionMatrixForAspect(float aspect) const;
  Vec3 Forward() const;
  Vec3 Right() const;
  Vec3 Up() const;

  Vec3 ScreenToWorldRayDir(float ndcX, float ndcY) const;

 private:
  CameraState state_;
  std::optional<CameraMatrixOverride> matrixOverride_;
  uint32_t viewportWidth_ = 1280;
  uint32_t viewportHeight_ = 720;
  float accelerationFactor_ = 1.0f;

  void ClearMatrixOverride();
  void SetPoseFromForwardUp(const Vec3& position, const Vec3& forward, const Vec3& up);
  void ClampPitch();
};

}  // namespace directxsplat
