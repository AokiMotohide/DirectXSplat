#include "app/CameraPathAnimator.h"

#include <algorithm>
#include <cmath>

#include "api/CameraSetInternal.h"

namespace directxsplat {

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

Vec3 RotateVector(const Quat& q, const Vec3& value) {
  const Vec3 u{q.x, q.y, q.z};
  const float s = q.w;
  return u * (2.0f * Dot(u, value)) + value * (s * s - Dot(u, u)) + Cross(u, value) * (2.0f * s);
}

Quat QuatMul(const Quat& a, const Quat& b) {
  return {
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
      a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
  };
}

Quat QuatConjugate(const Quat& q) {
  return {-q.x, -q.y, -q.z, q.w};
}

Quat QuatNegate(const Quat& q) {
  return {-q.x, -q.y, -q.z, -q.w};
}

Quat QuatFromAxisAngle(const Vec3& axis, float angle) {
  const float half = angle * 0.5f;
  const float s = std::sin(half);
  return Normalize({axis.x * s, axis.y * s, axis.z * s, std::cos(half)});
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
  out.pitch = -std::asin(std::clamp(forward.y, -1.0f, 1.0f));
  out.roll = WrapAngle(std::atan2(-Dot(inputUp, baseRight), Dot(inputUp, baseUp)));
  out.orbitPivot = out.position + forward * out.orbitDistance;
  return out;
}

CameraPathAnimator::Pose CameraPathAnimator::ExpPose(const PoseTangent& tangent) {
  const Vec3 w = tangent.rotation;
  const Vec3 v = tangent.translation;
  const float theta = Length(w);

  Pose out{};
  if (theta < 1e-10f) {
    out.orientation = {};
    out.position = v;
    return out;
  }

  out.orientation = QuatFromAxisAngle(w / theta, theta);
  const float s = std::sin(theta);
  const float c = std::cos(theta);
  const Vec3 wxv = Cross(w, v);
  const Vec3 wx2v = Cross(w, wxv);
  out.position = v + wxv * ((1.0f - c) / (theta * theta)) +
                 wx2v * ((theta - s) / (theta * theta * theta));
  return out;
}

CameraPathAnimator::PoseTangent CameraPathAnimator::LogPose(const Pose& pose) {
  const Quat q = SafeNormalize(pose.orientation);
  const float angle = 2.0f * std::acos(std::clamp(q.w, -1.0f, 1.0f));

  Vec3 axis{};
  if (angle >= 1e-10f) {
    const float s = std::sqrt(std::max(1.0f - q.w * q.w, 0.0f));
    if (s > 1e-10f) {
      axis = Vec3{q.x, q.y, q.z} / s;
    }
  }

  const Vec3 w = axis * angle;
  Vec3 translation = pose.position;
  if (angle > 1e-10f) {
    const float s = std::sin(angle);
    const float c = std::cos(angle);
    const float coeff = (1.0f / (angle * angle)) - ((1.0f + c) / (2.0f * angle * s));
    const Vec3 wxv = Cross(w, pose.position);
    const Vec3 wx2v = Cross(w, wxv);
    translation = pose.position - wxv * 0.5f + wx2v * coeff;
  }

  return {w, translation};
}

CameraPathAnimator::Pose CameraPathAnimator::InversePose(const Pose& pose) {
  Pose out{};
  out.orientation = QuatConjugate(SafeNormalize(pose.orientation));
  out.position = RotateVector(out.orientation, pose.position * -1.0f);
  return out;
}

CameraPathAnimator::Pose CameraPathAnimator::ComposePose(const Pose& a, const Pose& b) {
  Pose out{};
  out.orientation = SafeNormalize(QuatMul(SafeNormalize(a.orientation), SafeNormalize(b.orientation)));
  out.position = RotateVector(SafeNormalize(a.orientation), b.position) + a.position;
  return out;
}

CameraPathAnimator::Pose CameraPathAnimator::InterpolatePose(const Pose& p0, const Pose& p1,
                                                             const Pose& p2, const Pose& p3,
                                                             float t) {
  const auto add = [](const PoseTangent& a, const PoseTangent& b) {
    return PoseTangent{a.rotation + b.rotation, a.translation + b.translation};
  };
  const auto mul = [](const PoseTangent& a, float s) {
    return PoseTangent{a.rotation * s, a.translation * s};
  };

  Pose a0 = p0;
  Pose a1 = p1;
  Pose a2 = p2;
  Pose a3 = p3;
  if (DotQuat(a0.orientation, a1.orientation) < 0.0f) a1.orientation = QuatNegate(a1.orientation);
  if (DotQuat(a1.orientation, a2.orientation) < 0.0f) a2.orientation = QuatNegate(a2.orientation);
  if (DotQuat(a2.orientation, a3.orientation) < 0.0f) a3.orientation = QuatNegate(a3.orientation);

  const PoseTangent d01 = LogPose(ComposePose(InversePose(a0), a1));
  const PoseTangent d12 = LogPose(ComposePose(InversePose(a1), a2));
  const PoseTangent d23 = LogPose(ComposePose(InversePose(a2), a3));
  const PoseTangent m1 = mul(add(d01, d12), 0.5f);
  const PoseTangent m2 = mul(add(d12, d23), 0.5f);

  const float t2 = t * t;
  const float t3 = t2 * t;
  const float h1 = t3 - 2.0f * t2 + t;
  const float h2 = -2.0f * t3 + 3.0f * t2;
  const float h3 = t3 - t2;
  const PoseTangent tangent = add(add(mul(m1, h1), mul(d12, h2)), mul(m2, h3));
  return ComposePose(a1, ExpPose(tangent));
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
    poses_.push_back({input.position, SafeNormalize(input.rotation)});
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

  const Pose pose = InterpolatePose(poses_[i0], poses_[i1], poses_[i2], poses_[i3], u);
  outState = CameraStateFromPose(pose);
  return Finite(outState.position) && Finite(outState.yaw) && Finite(outState.pitch) && Finite(outState.roll) &&
         Finite(outState.fovYRadians);
}

}
