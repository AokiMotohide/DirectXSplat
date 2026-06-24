#include "app/CameraController.h"

#include <algorithm>
#include <cmath>

#include "api/CameraSetInternal.h"

namespace dxsplat {

namespace {

bool Finite(float v) {
  return std::isfinite(v);
}

bool Finite(const Vec3& v) {
  return Finite(v.x) && Finite(v.y) && Finite(v.z);
}

bool Finite(const Quat& q) {
  return Finite(q.x) && Finite(q.y) && Finite(q.z) && Finite(q.w);
}

Quat QuaternionFromYawPitch(float yaw, float pitch) {
  const float cy = std::cos(yaw * 0.5f);
  const float sy = std::sin(yaw * 0.5f);
  const float cp = std::cos(pitch * 0.5f);
  const float sp = std::sin(pitch * 0.5f);

  return Normalize({
      sp * cy,
      cp * sy,
      -sp * sy,
      cp * cy,
  });
}

Vec3 RotateVector(const Quat& q, const Vec3& v) {
  const Vec3 u{q.x, q.y, q.z};
  const float s = q.w;
  return u * (2.0f * Dot(u, v)) + v * (s * s - Dot(u, u)) + Cross(u, v) * (2.0f * s);
}

}  

CameraController::CameraController() = default;

void CameraController::ClearMatrixOverride() {
  matrixOverride_.reset();
}

void CameraController::SetViewport(uint32_t width, uint32_t height) {
  viewportWidth_ = std::max(width, 1u);
  viewportHeight_ = std::max(height, 1u);
}

void CameraController::SetState(const CameraState& state) {
  ClearMatrixOverride();
  state_ = state;
  if (!Finite(state_.position)) {
    state_.position = {};
  }
  if (!Finite(state_.yaw)) {
    state_.yaw = 0.0f;
  }
  if (!Finite(state_.pitch)) {
    state_.pitch = 0.0f;
  }
  if (!Finite(state_.roll)) {
    state_.roll = 0.0f;
  }
  if (state_.navigatorMode == NavigatorMode::Trackball) {
    state_.navigatorMode = NavigatorMode::Orbit;
  }
  state_.fovYRadians = Finite(state_.fovYRadians) ? std::clamp(state_.fovYRadians, 0.017453292f, 3.12413936f) : 1.0471975512f;
  state_.nearPlane = Finite(state_.nearPlane) ? std::max(state_.nearPlane, 0.0001f) : 0.1f;
  state_.farPlane = Finite(state_.farPlane) ? std::max(state_.farPlane, state_.nearPlane + 0.001f) : 5000.0f;
  state_.movementSpeed = Finite(state_.movementSpeed) ? std::max(state_.movementSpeed, 0.0f) : 2.5f;
  state_.rotationSpeed = Finite(state_.rotationSpeed) ? std::max(state_.rotationSpeed, 0.0f) : 1.0f;
  if (!Finite(state_.orbitPivot)) {
    state_.orbitPivot = {};
  }
  state_.orbitDistance = Finite(state_.orbitDistance) ? std::max(state_.orbitDistance, 0.01f) : 3.0f;
  ClampPitch();
}

const CameraState& CameraController::State() const { return state_; }

void CameraController::UpdateFps(float dt, bool moveForward, bool moveBackward, bool moveLeft, bool moveRight,
                                 bool moveUp, bool moveDown, float lookDeltaX, float lookDeltaY,
                                 float rollDelta, bool rotationEnabled) {
  if (rotationEnabled) {
    state_.yaw += lookDeltaX * 0.002f * state_.rotationSpeed;
    state_.pitch += lookDeltaY * 0.002f * state_.rotationSpeed;
    state_.roll += rollDelta * 0.002f * state_.rotationSpeed;
    ClampPitch();
  }

  const Vec3 f = Forward();
  const Vec3 r = Right();
  const Vec3 u{0.0f, 1.0f, 0.0f};

  Vec3 delta{};
  if (moveForward) delta = delta + f;
  if (moveBackward) delta = delta - f;
  if (moveRight) delta = delta + r;
  if (moveLeft) delta = delta - r;
  if (moveUp) delta = delta + u;
  if (moveDown) delta = delta - u;

  const bool moving = Length(delta) > 0.0f;
  if (rotationEnabled || moving) {
    ClearMatrixOverride();
  }
  if (state_.useAcceleration) {
    accelerationFactor_ = moving ? std::min(accelerationFactor_ * 1.02f, 50.0f) : 1.0f;
  } else {
    accelerationFactor_ = 1.0f;
  }

  if (moving) {
    delta = Normalize(delta) * (state_.movementSpeed * accelerationFactor_ * dt);
    state_.position = state_.position + delta;
  }

  if (state_.navigatorMode == NavigatorMode::Trackball || state_.navigatorMode == NavigatorMode::Orbit) {
    state_.orbitPivot = state_.position + f * state_.orbitDistance;
  }
}

void CameraController::UpdateOrbit(float, float orbitDeltaX, float orbitDeltaY, float panDeltaX, float panDeltaY,
                                    float wheelDelta) {
  if (orbitDeltaX != 0.0f || orbitDeltaY != 0.0f || panDeltaX != 0.0f || panDeltaY != 0.0f || wheelDelta != 0.0f) {
    ClearMatrixOverride();
  }
  state_.yaw += orbitDeltaX * 0.002f * state_.rotationSpeed;
  state_.pitch += orbitDeltaY * 0.002f * state_.rotationSpeed;
  ClampPitch();

  const Vec3 r = Right();
  const Vec3 u = Up();
  state_.orbitPivot = state_.orbitPivot - r * (panDeltaX * 0.01f) + u * (panDeltaY * 0.01f);

  state_.orbitDistance = std::max(0.01f, state_.orbitDistance * std::exp(-wheelDelta * 0.1f));
  state_.position = state_.orbitPivot - Forward() * state_.orbitDistance;
}

void CameraController::UpdateTrackball(float, float orbitDeltaX, float orbitDeltaY, float panDeltaX, float panDeltaY,
                                        float wheelDelta) {
  if (orbitDeltaX != 0.0f || orbitDeltaY != 0.0f || panDeltaX != 0.0f || panDeltaY != 0.0f || wheelDelta != 0.0f) {
    ClearMatrixOverride();
  }
  const Vec3 before = Normalize(state_.position - state_.orbitPivot);
  state_.yaw += orbitDeltaX * 0.002f * state_.rotationSpeed;
  state_.pitch += orbitDeltaY * 0.002f * state_.rotationSpeed;
  state_.roll += (orbitDeltaX * orbitDeltaY) * 0.000002f * state_.rotationSpeed;
  ClampPitch();

  const Vec3 r = Right();
  const Vec3 u = Up();
  const float panScale = std::max(0.001f, state_.orbitDistance * 0.002f);
  state_.orbitPivot = state_.orbitPivot - r * (panDeltaX * panScale) + u * (panDeltaY * panScale);
  state_.orbitDistance = std::max(0.01f, state_.orbitDistance * std::exp(-wheelDelta * 0.12f));
  const Vec3 after = Normalize(before + Right() * (-orbitDeltaX * 0.002f) + Up() * (orbitDeltaY * 0.002f));
  if (Length(after) > 0.0f) {
    state_.position = state_.orbitPivot + after * state_.orbitDistance;
    const Vec3 f = Normalize(state_.orbitPivot - state_.position);
    state_.yaw = std::atan2(f.x, f.z);
    state_.pitch = std::asin(std::clamp(f.y, -1.0f, 1.0f));
  } else {
    state_.position = state_.orbitPivot - Forward() * state_.orbitDistance;
  }
}

void CameraController::FocusBounds(const Aabb& bounds) {
  if (!bounds.valid) {
    return;
  }
  const Vec3 center = ComputeAabbCenter(bounds);
  const float radius = std::max(ComputeAabbRadius(bounds), 0.01f);
  FocusPoint(center, radius);
}

void CameraController::FocusPoint(const Vec3& point, float estimatedRadius) {
  ClearMatrixOverride();
  const float radius = std::max(estimatedRadius, 0.01f);
  state_.orbitPivot = point;
  const float halfFovY = state_.fovYRadians * 0.5f;
  const float aspect = static_cast<float>(std::max(viewportWidth_, 1u)) / static_cast<float>(std::max(viewportHeight_, 1u));
  const float halfFovX = std::atan(std::tan(halfFovY) * std::max(aspect, 0.01f));
  const float fitHalfFov = std::clamp(std::min(halfFovX, halfFovY), 0.05f, 1.5f);
  state_.orbitDistance = (radius / std::sin(fitHalfFov)) * 1.30f;
  if (state_.navigatorMode == NavigatorMode::Trackball || state_.navigatorMode == NavigatorMode::Orbit ||
      state_.navigatorMode == NavigatorMode::Interpolation) {
    state_.position = state_.orbitPivot - Forward() * state_.orbitDistance;
  } else {
    state_.position = point - Forward() * state_.orbitDistance;
  }
}

void CameraController::SetOrbitPivot(const Vec3& pivot) {
  ClearMatrixOverride();
  state_.orbitPivot = pivot;
  if (state_.navigatorMode == NavigatorMode::Trackball || state_.navigatorMode == NavigatorMode::Orbit ||
      state_.navigatorMode == NavigatorMode::Interpolation) {
    state_.orbitDistance = std::max(0.01f, Length(state_.position - state_.orbitPivot));
  }
}

void CameraController::SnapToInputCamera(const InputCamera& camera) {
  ClearMatrixOverride();
  if (!Finite(camera.position) || !Finite(camera.rotation)) {
    return;
  }
  const Quat q = Normalize(camera.rotation);
  if (!Finite(q) || (q.x == 0.0f && q.y == 0.0f && q.z == 0.0f && q.w == 0.0f)) {
    return;
  }
  const Vec3 f = RotateVector(q, {0.0f, 0.0f, 1.0f});
  const Vec3 inputUp = Normalize(RotateVector(q, {0.0f, 1.0f, 0.0f}));
  if (!Finite(f) || !Finite(inputUp) || Length(f) <= 1e-6f || Length(inputUp) <= 1e-6f) {
    return;
  }
  SetPoseFromForwardUp(camera.position, f, inputUp, camera.fovYRadians);
}

void CameraController::SnapToCameraParams(const CameraParams& camera) {
  if (!ValidateCameraParamsForRendering(camera).ok) {
    return;
  }

  const CameraRenderState renderState = CameraRenderStateFromCameraParams(camera, state_.nearPlane, state_.farPlane);
  const Vec3 forward = Normalize(Vec3{
      renderState.view.m[8],
      renderState.view.m[9],
      renderState.view.m[10],
  });
  const Vec3 up = Normalize(Vec3{
      renderState.view.m[4],
      renderState.view.m[5],
      renderState.view.m[6],
  });
  if (!Finite(renderState.position) || !Finite(forward) || !Finite(up) ||
      Length(forward) <= 1e-6f || Length(up) <= 1e-6f) {
    return;
  }

  SetPoseFromForwardUp(renderState.position, forward, up, renderState.fovYRadians);
  matrixOverride_ = CameraMatrixOverride{renderState.view, renderState.proj, renderState.position};
}

bool CameraController::SnapToClosestInputCamera(const std::vector<InputCamera>& cameras) {
  if (cameras.empty()) {
    return false;
  }

  size_t best = 0;
  float bestDist2 = std::numeric_limits<float>::max();
  for (size_t i = 0; i < cameras.size(); ++i) {
    const Vec3 d = cameras[i].position - state_.position;
    const float dist2 = Dot(d, d);
    if (dist2 < bestDist2) {
      bestDist2 = dist2;
      best = i;
    }
  }

  SnapToInputCamera(cameras[best]);
  return true;
}

Mat4 CameraController::ViewMatrix() const {
  if (matrixOverride_.has_value()) {
    return matrixOverride_->view;
  }
  return LookAt(state_.position, state_.position + Forward(), Up());
}

Mat4 CameraController::ProjectionMatrix() const {
  const float aspect = static_cast<float>(viewportWidth_) / static_cast<float>(viewportHeight_);
  return ProjectionMatrixForAspect(aspect);
}

Mat4 CameraController::ProjectionMatrixForAspect(float aspect) const {
  if (matrixOverride_.has_value()) {
    return matrixOverride_->proj;
  }
  return Perspective(state_.fovYRadians, std::max(aspect, 0.01f), state_.nearPlane, state_.farPlane);
}

Vec3 CameraController::Forward() const {
  const Quat q = QuaternionFromYawPitch(state_.yaw, state_.pitch);
  return Normalize(RotateVector(q, {0.0f, 0.0f, 1.0f}));
}

Vec3 CameraController::Right() const {
  const Vec3 f = Forward();
  const Vec3 baseRight = Normalize(Cross({0.0f, 1.0f, 0.0f}, f));
  const Vec3 baseUp = Normalize(Cross(f, baseRight));
  return Normalize(baseRight * std::cos(state_.roll) + baseUp * std::sin(state_.roll));
}

Vec3 CameraController::Up() const {
  const Vec3 f = Forward();
  const Vec3 baseRight = Normalize(Cross({0.0f, 1.0f, 0.0f}, f));
  const Vec3 baseUp = Normalize(Cross(f, baseRight));
  return Normalize(baseUp * std::cos(state_.roll) - baseRight * std::sin(state_.roll));
}

Vec3 CameraController::ScreenToWorldRayDir(float ndcX, float ndcY) const {
  Mat4 invProj = Inverse(ProjectionMatrix());
  Mat4 invView = Inverse(ViewMatrix());
  Vec4 clip{ndcX, ndcY, 1.0f, 1.0f};
  Vec4 view = Mul(invProj, clip);
  if (std::abs(view.w) > 1e-6f) {
    view.x /= view.w;
    view.y /= view.w;
    view.z /= view.w;
  }
  view.w = 0.0f;
  Vec4 world = Mul(invView, view);
  return Normalize(Vec3{world.x, world.y, world.z});
}

void CameraController::SetPoseFromForwardUp(const Vec3& position, const Vec3& forward, const Vec3& up, float fovYRadians) {
  const Vec3 f = Normalize(forward);
  const Vec3 inputUp = Normalize(up);
  if (!Finite(position) || !Finite(f) || !Finite(inputUp) || Length(f) <= 1e-6f || Length(inputUp) <= 1e-6f) {
    return;
  }

  state_.position = position;
  state_.yaw = std::atan2(f.x, f.z);
  state_.pitch = std::asin(std::clamp(f.y, -1.0f, 1.0f));
  const Vec3 baseForward = Forward();
  const Vec3 baseRight = Normalize(Cross({0.0f, 1.0f, 0.0f}, baseForward));
  const Vec3 baseUp = Normalize(Cross(baseForward, baseRight));
  if (!Finite(baseRight) || !Finite(baseUp) || Length(baseRight) <= 1e-6f || Length(baseUp) <= 1e-6f) {
    state_.roll = 0.0f;
  } else {
    state_.roll = std::atan2(-Dot(inputUp, baseRight), Dot(inputUp, baseUp));
  }
  state_.fovYRadians = Finite(fovYRadians) ? fovYRadians : 1.0471975512f;
  state_.orbitPivot = state_.position + f * state_.orbitDistance;
  SetState(state_);
}

void CameraController::ClampPitch() { state_.pitch = std::clamp(state_.pitch, -1.55334f, 1.55334f); }

}  // namespace dxsplat
