#include "app/CameraController.h"

#include <algorithm>
#include <cmath>

namespace dxsplat {

namespace {

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

Quat QuaternionFromBasis(const Vec3& right, const Vec3& up, const Vec3& forward) {
  const float m00 = right.x;
  const float m01 = up.x;
  const float m02 = forward.x;
  const float m10 = right.y;
  const float m11 = up.y;
  const float m12 = forward.y;
  const float m20 = right.z;
  const float m21 = up.z;
  const float m22 = forward.z;

  const float trace = m00 + m11 + m22;
  Quat out{};
  if (trace > 0.0f) {
    const float s = std::sqrt(trace + 1.0f) * 2.0f;
    out.w = 0.25f * s;
    out.x = (m21 - m12) / s;
    out.y = (m02 - m20) / s;
    out.z = (m10 - m01) / s;
  } else if (m00 > m11 && m00 > m22) {
    const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
    out.w = (m21 - m12) / s;
    out.x = 0.25f * s;
    out.y = (m01 + m10) / s;
    out.z = (m02 + m20) / s;
  } else if (m11 > m22) {
    const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
    out.w = (m02 - m20) / s;
    out.x = (m01 + m10) / s;
    out.y = 0.25f * s;
    out.z = (m12 + m21) / s;
  } else {
    const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
    out.w = (m10 - m01) / s;
    out.x = (m02 + m20) / s;
    out.y = (m12 + m21) / s;
    out.z = 0.25f * s;
  }
  return Normalize(out);
}

Vec3 RotateVector(const Quat& q, const Vec3& v) {
  const Vec3 u{q.x, q.y, q.z};
  const float s = q.w;
  return u * (2.0f * Dot(u, v)) + v * (s * s - Dot(u, u)) + Cross(u, v) * (2.0f * s);
}

}  

CameraController::CameraController() = default;

void CameraController::SetViewport(uint32_t width, uint32_t height) {
  viewportWidth_ = std::max(width, 1u);
  viewportHeight_ = std::max(height, 1u);
}

void CameraController::SetState(const CameraState& state) {
  state_ = state;
  if (state_.navigatorMode == NavigatorMode::Trackball) {
    state_.navigatorMode = NavigatorMode::Orbit;
  }
  state_.fovYRadians = std::clamp(state_.fovYRadians, 0.017453292f, 3.12413936f);
  state_.nearPlane = std::max(state_.nearPlane, 0.0001f);
  state_.farPlane = std::max(state_.farPlane, state_.nearPlane + 0.001f);
  state_.movementSpeed = std::max(state_.movementSpeed, 0.0f);
  state_.rotationSpeed = std::max(state_.rotationSpeed, 0.0f);
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
  state_.orbitPivot = pivot;
  if (state_.navigatorMode == NavigatorMode::Trackball || state_.navigatorMode == NavigatorMode::Orbit ||
      state_.navigatorMode == NavigatorMode::Interpolation) {
    state_.orbitDistance = std::max(0.01f, Length(state_.position - state_.orbitPivot));
  }
}

void CameraController::SnapToInputCamera(const InputCamera& camera) {
  state_.position = camera.position;
  const Quat q = Normalize(camera.rotation);
  const Vec3 f = RotateVector(q, {0.0f, 0.0f, 1.0f});
  const Vec3 inputUp = Normalize(RotateVector(q, {0.0f, 1.0f, 0.0f}));
  state_.yaw = std::atan2(f.x, f.z);
  state_.pitch = std::asin(std::clamp(f.y, -1.0f, 1.0f));
  const Vec3 baseForward = Forward();
  const Vec3 baseRight = Normalize(Cross({0.0f, 1.0f, 0.0f}, baseForward));
  const Vec3 baseUp = Normalize(Cross(baseForward, baseRight));
  state_.roll = std::atan2(-Dot(inputUp, baseRight), Dot(inputUp, baseUp));
  state_.fovYRadians = camera.fovYRadians;
  state_.orbitPivot = state_.position + f * state_.orbitDistance;
}

void CameraController::SnapToCameraParams(const CameraParams& camera) {
  const auto& e = camera.extrinsic;
  const Vec3 right = Normalize(Vec3{e[0], e[1], e[2]});
  const Vec3 down = Normalize(Vec3{e[4], e[5], e[6]});
  const Vec3 forward = Normalize(Vec3{e[8], e[9], e[10]});

  InputCamera input{};
  input.name = camera.name;
  input.position = right * (-e[3]) + down * (-e[7]) + forward * (-e[11]);
  input.rotation = QuaternionFromBasis(right, down * -1.0f, forward);
  if (camera.height > 0 && camera.intrinsic[4] > 0.0f) {
    input.fovYRadians = 2.0f * std::atan(static_cast<float>(camera.height) * 0.5f / camera.intrinsic[4]);
  }
  SnapToInputCamera(input);
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

Mat4 CameraController::ViewMatrix() const { return LookAt(state_.position, state_.position + Forward(), Up()); }

Mat4 CameraController::ProjectionMatrix() const {
  const float aspect = static_cast<float>(viewportWidth_) / static_cast<float>(viewportHeight_);
  return ProjectionMatrixForAspect(aspect);
}

Mat4 CameraController::ProjectionMatrixForAspect(float aspect) const {
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

void CameraController::ClampPitch() { state_.pitch = std::clamp(state_.pitch, -1.55334f, 1.55334f); }

}  // namespace dxsplat
