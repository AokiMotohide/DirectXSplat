#include "app/CameraPathAnimator.h"

#include <algorithm>
#include <cmath>

#include "api/CameraSetInternal.h"

namespace dxsplat {

namespace {

constexpr float kTwoPi = 6.28318530718f;

bool Finite(float value) {
  return std::isfinite(value);
}

bool Finite(const Vec3& value) {
  return Finite(value.x) && Finite(value.y) && Finite(value.z);
}

bool Finite(const Quat& value) {
  return Finite(value.x) && Finite(value.y) && Finite(value.z) && Finite(value.w);
}

float WrapTime(float time, size_t count) {
  if (count == 0 || !Finite(time)) {
    return 0.0f;
  }
  const float length = static_cast<float>(count);
  time = std::fmod(time, length);
  return time < 0.0f ? time + length : time;
}

Vec3 SafeNormalize(const Vec3& value, const Vec3& fallback) {
  if (!Finite(value)) {
    return fallback;
  }
  const float len = Length(value);
  if (!Finite(len) || len <= 1e-6f) {
    return fallback;
  }
  return value / len;
}

Quat SafeNormalize(const Quat& value) {
  const Quat normalized = Normalize(value);
  if (!Finite(normalized) || (normalized.x == 0.0f && normalized.y == 0.0f &&
                              normalized.z == 0.0f && normalized.w == 0.0f)) {
    return {};
  }
  return normalized;
}

float DotQuat(const Quat& a, const Quat& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Quat Negate(const Quat& q) {
  return {-q.x, -q.y, -q.z, -q.w};
}

Quat Slerp(Quat a, Quat b, float t) {
  a = SafeNormalize(a);
  b = SafeNormalize(b);
  float dot = DotQuat(a, b);
  if (dot < 0.0f) {
    b = Negate(b);
    dot = -dot;
  }
  dot = std::clamp(dot, -1.0f, 1.0f);

  if (dot > 0.9995f) {
    return SafeNormalize({
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t,
    });
  }

  const float theta = std::acos(dot);
  const float sinTheta = std::sin(theta);
  if (std::abs(sinTheta) <= 1e-6f) {
    return a;
  }

  const float wa = std::sin((1.0f - t) * theta) / sinTheta;
  const float wb = std::sin(t * theta) / sinTheta;
  return SafeNormalize({
      a.x * wa + b.x * wb,
      a.y * wa + b.y * wb,
      a.z * wa + b.z * wb,
      a.w * wa + b.w * wb,
  });
}

Vec3 CatmullRom(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t) {
  const float t2 = t * t;
  const float t3 = t2 * t;
  return {
      0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
              (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
              (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
      0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
              (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
              (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3),
      0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * t +
              (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 +
              (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3),
  };
}

Vec3 RotateVector(const Quat& q, const Vec3& value) {
  const Vec3 u{q.x, q.y, q.z};
  const float s = q.w;
  return u * (2.0f * Dot(u, value)) + value * (s * s - Dot(u, u)) + Cross(u, value) * (2.0f * s);
}

float WrapAngle(float value) {
  return Finite(value) ? std::remainder(value, kTwoPi) : 0.0f;
}

void BuildViewBasis(const Vec3& forward, Vec3& right, Vec3& up) {
  const Vec3 f = SafeNormalize(forward, {0.0f, 0.0f, 1.0f});
  const Vec3 referenceUp = std::abs(f.y) > 0.98f ? Vec3{0.0f, 0.0f, 1.0f} : Vec3{0.0f, 1.0f, 0.0f};
  right = SafeNormalize(Cross(referenceUp, f), {1.0f, 0.0f, 0.0f});
  up = SafeNormalize(Cross(f, right), {0.0f, 1.0f, 0.0f});
}

}

CameraState CameraPathAnimator::CameraStateFromPose(const Pose& pose) {
  CameraState out{};
  const Quat q = SafeNormalize(pose.orientation);
  const Vec3 forward = SafeNormalize(RotateVector(q, {0.0f, 0.0f, 1.0f}), {0.0f, 0.0f, 1.0f});
  const Vec3 inputUp = SafeNormalize(RotateVector(q, {0.0f, 1.0f, 0.0f}), {0.0f, 1.0f, 0.0f});
  Vec3 baseRight{};
  Vec3 baseUp{};
  BuildViewBasis(forward, baseRight, baseUp);

  out.position = Finite(pose.position) ? pose.position : Vec3{};
  out.yaw = WrapAngle(std::atan2(forward.x, forward.z));
  out.pitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
  out.roll = WrapAngle(std::atan2(-Dot(inputUp, baseRight), Dot(inputUp, baseUp)));
  out.fovYRadians = Finite(pose.fovYRadians) ? std::clamp(pose.fovYRadians, 0.017453292f, 3.12413936f)
                                             : 1.0471975512f;
  out.orbitPivot = out.position + forward * out.orbitDistance;
  return out;
}

void ClampAnimationUiState(AnimationUiState& state, size_t cameraCount) {
  state.fps = Finite(state.fps) ? std::clamp(state.fps, 1.0f, 30.0f) : 1.0f;
  state.time = WrapTime(state.time, cameraCount);
  if (cameraCount == 0) {
    state.enabled = false;
  }
}

void StopAnimationOnCameraEdit(AnimationUiState& state, bool cameraEdited) {
  if (cameraEdited) {
    state.enabled = false;
  }
}

void CameraPathAnimator::SetCameras(const CameraSet& cameras) {
  poses_.clear();
  poses_.reserve(cameras.cameras.size());

  for (size_t i = 0; i < cameras.cameras.size(); ++i) {
    const CameraParams& camera = cameras.cameras[i];
    if (!ValidateCameraParamsForRendering(camera).ok) {
      continue;
    }
    const InputCamera input = InputCameraFromCameraParams(camera, i);
    if (!Finite(input.position) || !Finite(input.rotation)) {
      continue;
    }
    poses_.push_back({input.position, SafeNormalize(input.rotation), input.fovYRadians});
  }

  time_ = WrapTime(time_, poses_.size());
}

void CameraPathAnimator::SetTime(float time) {
  time_ = WrapTime(time, poses_.size());
}

float CameraPathAnimator::Time() const {
  return time_;
}

void CameraPathAnimator::Advance(float dt, float fps) {
  if (poses_.empty()) {
    time_ = 0.0f;
    return;
  }
  if (!Finite(dt)) {
    dt = 0.0f;
  }
  if (!Finite(fps)) {
    fps = 1.0f;
  }
  time_ = WrapTime(time_ + std::max(dt, 0.0f) * std::clamp(fps, 1.0f, 30.0f), poses_.size());
}

bool CameraPathAnimator::Evaluate(CameraState& outState) const {
  if (poses_.empty()) {
    return false;
  }
  if (poses_.size() == 1) {
    outState = CameraStateFromPose(poses_[0]);
    return true;
  }

  const float t = WrapTime(time_, poses_.size());
  const size_t i1 = static_cast<size_t>(std::floor(t)) % poses_.size();
  const size_t i0 = (i1 + poses_.size() - 1u) % poses_.size();
  const size_t i2 = (i1 + 1u) % poses_.size();
  const size_t i3 = (i1 + 2u) % poses_.size();
  const float u = t - std::floor(t);

  Pose pose{};
  pose.position = CatmullRom(poses_[i0].position, poses_[i1].position, poses_[i2].position, poses_[i3].position, u);
  pose.orientation = Slerp(poses_[i1].orientation, poses_[i2].orientation, u);
  pose.fovYRadians = poses_[i1].fovYRadians + (poses_[i2].fovYRadians - poses_[i1].fovYRadians) * u;
  outState = CameraStateFromPose(pose);
  return Finite(outState.position) && Finite(outState.yaw) && Finite(outState.pitch) && Finite(outState.roll) &&
         Finite(outState.fovYRadians);
}

}
