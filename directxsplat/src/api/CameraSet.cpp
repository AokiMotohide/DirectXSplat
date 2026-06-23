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

std::array<float, 16> BuildOpenCvExtrinsic(const InputCamera& camera) {
  const Quat q = Normalize(camera.rotation);
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

}  // namespace

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
  const Vec3 right = Normalize(Vec3{e[0], e[1], e[2]});
  const Vec3 down = Normalize(Vec3{e[4], e[5], e[6]});
  const Vec3 forward = Normalize(Vec3{e[8], e[9], e[10]});
  const Vec3 up = down * -1.0f;

  InputCamera out{};
  out.name = camera.name.empty() ? "camera " + std::to_string(index) : camera.name;
  out.position = right * (-e[3]) + down * (-e[7]) + forward * (-e[11]);
  out.rotation = QuaternionFromBasis(right, up, forward);
  if (camera.height > 0 && camera.intrinsic[4] > 0.0f) {
    out.fovYRadians = 2.0f * std::atan(static_cast<float>(camera.height) * 0.5f / camera.intrinsic[4]);
  }
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
