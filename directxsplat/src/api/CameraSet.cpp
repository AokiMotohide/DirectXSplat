#include "api/CameraSetInternal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace dxsplat {

namespace {

constexpr uint32_t kDefaultCameraWidth = 1600;
constexpr uint32_t kDefaultCameraHeight = 900;
constexpr uint32_t kMaxCameraDimension = 1u << 20u;
constexpr float kCameraBasisEpsilon = 1e-5f;
constexpr float kCameraOrthonormalTolerance = 0.05f;
constexpr float kCameraPrincipalPointSlack = 4.0f;

bool Finite(float v) {
  return std::isfinite(v);
}

bool Finite(const Vec3& v) {
  return Finite(v.x) && Finite(v.y) && Finite(v.z);
}

bool Finite(const Quat& q) {
  return Finite(q.x) && Finite(q.y) && Finite(q.z) && Finite(q.w);
}

template <size_t Count>
bool Finite(const std::array<float, Count>& values) {
  for (float value : values) {
    if (!Finite(value)) {
      return false;
    }
  }
  return true;
}

Vec3 RotateVector(const Quat& q, const Vec3& v) {
  const Vec3 u{q.x, q.y, q.z};
  const float s = q.w;
  return u * (2.0f * Dot(u, v)) + v * (s * s - Dot(u, u)) + Cross(u, v) * (2.0f * s);
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
    if (!Finite(s) || std::abs(s) <= 1e-6f) {
      return {};
    }
    out.w = 0.25f * s;
    out.x = (m21 - m12) / s;
    out.y = (m02 - m20) / s;
    out.z = (m10 - m01) / s;
  } else if (m00 > m11 && m00 > m22) {
    const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
    if (!Finite(s) || std::abs(s) <= 1e-6f) {
      return {};
    }
    out.w = (m21 - m12) / s;
    out.x = 0.25f * s;
    out.y = (m01 + m10) / s;
    out.z = (m02 + m20) / s;
  } else if (m11 > m22) {
    const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
    if (!Finite(s) || std::abs(s) <= 1e-6f) {
      return {};
    }
    out.w = (m02 - m20) / s;
    out.x = (m01 + m10) / s;
    out.y = 0.25f * s;
    out.z = (m12 + m21) / s;
  } else {
    const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
    if (!Finite(s) || std::abs(s) <= 1e-6f) {
      return {};
    }
    out.w = (m10 - m01) / s;
    out.x = (m02 + m20) / s;
    out.y = (m12 + m21) / s;
    out.z = 0.25f * s;
  }
  const Quat normalized = Normalize(out);
  return Finite(normalized) ? normalized : Quat{};
}

std::array<float, 16> BuildOpenCvExtrinsic(const InputCamera& camera) {
  Quat q = Normalize(camera.rotation);
  if (!Finite(q) || (q.x == 0.0f && q.y == 0.0f && q.z == 0.0f && q.w == 0.0f)) {
    q = {};
  }
  const Vec3 right = RotateVector(q, {1.0f, 0.0f, 0.0f});
  const Vec3 down = RotateVector(q, {0.0f, -1.0f, 0.0f});
  const Vec3 forward = RotateVector(q, {0.0f, 0.0f, 1.0f});
  const Vec3 origin = camera.position;

  return {
      right.x, right.y, right.z, -Dot(right, origin),
      down.x, down.y, down.z, -Dot(down, origin),
      forward.x, forward.y, forward.z, -Dot(forward, origin),
      0.0f, 0.0f, 0.0f, 1.0f,
  };
}

Vec3 CameraPositionFromExtrinsic(const std::array<float, 16>& e) {
  const Vec3 right{e[0], e[1], e[2]};
  const Vec3 down{e[4], e[5], e[6]};
  const Vec3 forward{e[8], e[9], e[10]};
  return right * (-e[3]) + down * (-e[7]) + forward * (-e[11]);
}

float FovYFromIntrinsics(const CameraParams& camera) {
  if (camera.height == 0 || camera.intrinsic[4] <= 0.0f) {
    return 1.0471975512f;
  }
  return 2.0f * std::atan(static_cast<float>(camera.height) * 0.5f / camera.intrinsic[4]);
}

std::array<float, 9> BuildIntrinsic(uint32_t width, uint32_t height, float fovYRadians) {
  const float safeFovY = std::clamp(fovYRadians, 0.001f, 3.13f);
  const float fy = static_cast<float>(height) * 0.5f / std::tan(safeFovY * 0.5f);
  const float fx = fy;
  const float cx = static_cast<float>(width) * 0.5f;
  const float cy = static_cast<float>(height) * 0.5f;
  return {
      fx, 0.0f, cx,
      0.0f, fy, cy,
      0.0f, 0.0f, 1.0f,
  };
}

bool ValidCameraBasis(const std::array<float, 16>& e) {
  const Vec3 right{e[0], e[1], e[2]};
  const Vec3 down{e[4], e[5], e[6]};
  const Vec3 forward{e[8], e[9], e[10]};
  const float rightLen = Length(right);
  const float downLen = Length(down);
  const float forwardLen = Length(forward);
  if (rightLen <= kCameraBasisEpsilon || downLen <= kCameraBasisEpsilon || forwardLen <= kCameraBasisEpsilon) {
    return false;
  }

  const Vec3 rightN = right / rightLen;
  const Vec3 downN = down / downLen;
  const Vec3 forwardN = forward / forwardLen;
  const float det = Dot(rightN, Cross(downN, forwardN));
  return std::abs(Dot(rightN, downN)) <= kCameraOrthonormalTolerance &&
         std::abs(Dot(rightN, forwardN)) <= kCameraOrthonormalTolerance &&
         std::abs(Dot(downN, forwardN)) <= kCameraOrthonormalTolerance &&
         std::abs(std::abs(det) - 1.0f) <= kCameraOrthonormalTolerance;
}

}  // namespace

Status ValidateCameraParamsForRendering(const CameraParams& camera) {
  if (camera.width == 0) {
    return Status::Error("camera width must be greater than zero");
  }
  if (camera.height == 0) {
    return Status::Error("camera height must be greater than zero");
  }
  if (camera.width > kMaxCameraDimension || camera.height > kMaxCameraDimension) {
    return Status::Error("invalid camera dimensions");
  }
  if (!Finite(camera.extrinsic) || !Finite(camera.intrinsic)) {
    return Status::Error("invalid camera matrix");
  }
  if (!ValidCameraBasis(camera.extrinsic) ||
      std::abs(camera.extrinsic[12]) > kCameraBasisEpsilon ||
      std::abs(camera.extrinsic[13]) > kCameraBasisEpsilon ||
      std::abs(camera.extrinsic[14]) > kCameraBasisEpsilon ||
      std::abs(camera.extrinsic[15] - 1.0f) > kCameraOrthonormalTolerance) {
    return Status::Error("invalid camera extrinsic");
  }

  const float width = static_cast<float>(camera.width);
  const float height = static_cast<float>(camera.height);
  if (camera.intrinsic[0] <= kCameraBasisEpsilon || camera.intrinsic[4] <= kCameraBasisEpsilon ||
      std::abs(camera.intrinsic[8]) <= kCameraBasisEpsilon ||
      std::abs(camera.intrinsic[2]) > width * kCameraPrincipalPointSlack ||
      std::abs(camera.intrinsic[5]) > height * kCameraPrincipalPointSlack) {
    return Status::Error("invalid camera intrinsic");
  }
  return Status::Ok();
}

CameraRenderState CameraRenderStateFromCameraParams(const CameraParams& camera, float nearPlane, float farPlane) {
  const float safeNear = Finite(nearPlane) ? std::max(nearPlane, 0.0001f) : 0.1f;
  const float safeFar = Finite(farPlane) ? std::max(farPlane, safeNear + 0.001f) : 5000.0f;
  const float zRange = safeFar - safeNear;
  const float width = static_cast<float>(std::max(camera.width, 1u));
  const float height = static_cast<float>(std::max(camera.height, 1u));
  const auto& e = camera.extrinsic;

  CameraRenderState out{};
  out.view.m = {
      e[0], e[1], e[2], e[3],
      -e[4], -e[5], -e[6], -e[7],
      e[8], e[9], e[10], e[11],
      0.0f, 0.0f, 0.0f, 1.0f,
  };
  out.proj.m = {
      2.0f * camera.intrinsic[0] / width, 0.0f, (2.0f * camera.intrinsic[2] / width) - 1.0f, 0.0f,
      0.0f, -2.0f * camera.intrinsic[4] / height, 1.0f - (2.0f * camera.intrinsic[5] / height), 0.0f,
      0.0f, 0.0f, safeFar / zRange, -safeNear * safeFar / zRange,
      0.0f, 0.0f, 1.0f, 0.0f,
  };
  out.position = CameraPositionFromExtrinsic(camera.extrinsic);
  return out;
}

CameraParams CameraParamsFromInputCamera(const InputCamera& input, uint32_t width, uint32_t height) {
  CameraParams camera{};
  camera.name = input.name;
  if (input.hasMatrixParams && input.width > 0 && input.height > 0) {
    camera.width = input.width;
    camera.height = input.height;
    camera.extrinsic = input.extrinsic.m;
    camera.intrinsic = input.intrinsic.m;
    return camera;
  }
  camera.width = width;
  camera.height = height;
  camera.extrinsic = BuildOpenCvExtrinsic(input);
  camera.intrinsic = BuildIntrinsic(width, height, input.fovYRadians);
  return camera;
}

InputCamera InputCameraFromCameraParams(const CameraParams& camera, size_t index) {
  const auto& e = camera.extrinsic;
  const Vec3 down = Normalize(Vec3{e[4], e[5], e[6]});
  const Vec3 forward = Normalize(Vec3{e[8], e[9], e[10]});
  const Vec3 up = down * -1.0f;
  Vec3 right = Normalize(Cross(up, forward));
  Vec3 orthoUp = Normalize(Cross(forward, right));
  if (!Finite(right) || Length(right) <= 1e-6f || !Finite(orthoUp) || Length(orthoUp) <= 1e-6f) {
    right = Normalize(Vec3{e[0], e[1], e[2]});
    orthoUp = up;
  }

  InputCamera out{};
  out.name = camera.name.empty() ? "camera " + std::to_string(index) : camera.name;
  if (Finite(right) && Finite(orthoUp) && Finite(forward) &&
      Length(right) > 1e-6f && Length(orthoUp) > 1e-6f && Length(forward) > 1e-6f) {
    out.position = CameraPositionFromExtrinsic(camera.extrinsic);
    out.rotation = QuaternionFromBasis(right, orthoUp, forward);
  }
  out.fovYRadians = FovYFromIntrinsics(camera);
  out.extrinsic.m = camera.extrinsic;
  out.intrinsic.m = camera.intrinsic;
  out.width = camera.width;
  out.height = camera.height;
  out.hasMatrixParams = camera.width > 0 && camera.height > 0;
  return out;
}

StatusOr<CameraSet> ConvertInputCamerasToCameraSet(const Scene& scene) {
  CameraSet out{};
  out.cameras.reserve(scene.inputCameras.size());

  // Convert DirectXSplat cameras into public OpenCV matrices
  for (const auto& input : scene.inputCameras) {
    out.cameras.push_back(CameraParamsFromInputCamera(input, kDefaultCameraWidth, kDefaultCameraHeight));
  }

  return StatusOr<CameraSet>::Ok(std::move(out));
}

}  // namespace dxsplat
