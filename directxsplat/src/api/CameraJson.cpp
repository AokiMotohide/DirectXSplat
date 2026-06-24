#include "dxsplat/directxsplat.h"

#include <array>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "api/CameraSetInternal.h"

namespace dxsplat {

namespace {

constexpr uint32_t kDefaultCameraWidth = 1600;
constexpr uint32_t kDefaultCameraHeight = 900;
constexpr size_t kMaxCameraStringBytes = 4096;

using Json = nlohmann::json;

bool IsFinite(float v) {
  return std::isfinite(v);
}

template <size_t Count>
bool IsFinite(const std::array<float, Count>& values) {
  for (float value : values) {
    if (!IsFinite(value)) {
      return false;
    }
  }
  return true;
}

Status ValidateLoadedCameraParams(const CameraParams& camera) {
  if (!IsFinite(camera.extrinsic) || !IsFinite(camera.intrinsic)) {
    return Status::Error("invalid camera matrix");
  }
  if (camera.intrinsic[0] <= 0.0f || camera.intrinsic[4] <= 0.0f || std::abs(camera.intrinsic[8]) <= 1e-5f) {
    return Status::Error("invalid camera intrinsic");
  }
  return Status::Ok();
}

std::string ReadCameraName(const Json& item) {
  const auto it = item.find("name");
  if (it == item.end() || !it->is_string()) {
    return "camera";
  }
  const std::string& value = it->get_ref<const std::string&>();
  if (value.size() > kMaxCameraStringBytes) {
    return "camera";
  }
  return value;
}

bool ReadVec3(const Json& item, const char* name, Vec3& out) {
  const auto it = item.find(name);
  if (it == item.end()) {
    return true;
  }
  if (!it->is_array() || it->size() < 3) {
    return false;
  }
  out = {it->at(0).get<float>(), it->at(1).get<float>(), it->at(2).get<float>()};
  return true;
}

bool ReadQuat(const Json& item, const char* name, Quat& out) {
  const auto it = item.find(name);
  if (it == item.end()) {
    return true;
  }
  if (!it->is_array() || it->size() < 4) {
    return false;
  }
  out = Normalize({it->at(0).get<float>(), it->at(1).get<float>(), it->at(2).get<float>(), it->at(3).get<float>()});
  return true;
}

template <size_t Rows, size_t Cols>
StatusOr<std::array<float, Rows * Cols>> ReadMatrix(const Json& item, const char* name) {
  const auto it = item.find(name);
  if (it == item.end() || !it->is_array() || it->size() != Rows) {
    return StatusOr<std::array<float, Rows * Cols>>::Error("invalid camera matrix");
  }

  std::array<float, Rows * Cols> out{};
  for (size_t row = 0; row < Rows; ++row) {
    const Json& rowJson = it->at(row);
    if (!rowJson.is_array() || rowJson.size() != Cols) {
      return StatusOr<std::array<float, Rows * Cols>>::Error("invalid camera matrix");
    }
    for (size_t col = 0; col < Cols; ++col) {
      const float value = rowJson.at(col).get<float>();
      if (!IsFinite(value)) {
        return StatusOr<std::array<float, Rows * Cols>>::Error("invalid camera matrix");
      }
      out[row * Cols + col] = value;
    }
  }
  return StatusOr<std::array<float, Rows * Cols>>::Ok(out);
}

StatusOr<CameraParams> ParseMatrixCamera(const Json& item) {
  auto extrinsic = ReadMatrix<4, 4>(item, "extrinsic");
  if (!extrinsic.ok()) {
    return StatusOr<CameraParams>::Error(extrinsic.status.message);
  }
  auto intrinsic = ReadMatrix<3, 3>(item, "intrinsic");
  if (!intrinsic.ok()) {
    return StatusOr<CameraParams>::Error(intrinsic.status.message);
  }

  CameraParams camera{};
  camera.name = ReadCameraName(item);
  camera.extrinsic = extrinsic.value;
  camera.intrinsic = intrinsic.value;
  camera.width = item.value("width", 0u);
  camera.height = item.value("height", 0u);
  if (camera.width == 0 || camera.height == 0) {
    return StatusOr<CameraParams>::Error("invalid camera dimensions");
  }
  Status validation = ValidateLoadedCameraParams(camera);
  if (!validation.ok) {
    return StatusOr<CameraParams>::Error(validation.message);
  }
  return StatusOr<CameraParams>::Ok(std::move(camera));
}

StatusOr<CameraParams> ParseDirectXSplatCamera(const Json& item) {
  InputCamera input{};
  input.name = ReadCameraName(item);
  if (!ReadVec3(item, "position", input.position) || !ReadQuat(item, "rotation", input.rotation)) {
    return StatusOr<CameraParams>::Error("invalid camera json");
  }
  input.fovYRadians = item.value("fovY", input.fovYRadians);
  const uint32_t width = item.value("width", kDefaultCameraWidth);
  const uint32_t height = item.value("height", kDefaultCameraHeight);
  return StatusOr<CameraParams>::Ok(CameraParamsFromInputCamera(input, width, height));
}

}  // namespace

StatusOr<CameraSet> LoadCameraSet(const std::filesystem::path& cameraJsonPath) {
  if (cameraJsonPath.empty()) {
    return StatusOr<CameraSet>::Error("camera path is empty");
  }

  std::ifstream file(cameraJsonPath);
  if (!file.is_open()) {
    return StatusOr<CameraSet>::Error("failed to open camera json");
  }

  Json root;
  try {
    file >> root;
  } catch (const nlohmann::json::exception&) {
    return StatusOr<CameraSet>::Error("invalid camera json");
  }

  if (!root.is_array()) {
    return StatusOr<CameraSet>::Error("camera json must be an array");
  }

  CameraSet out{};
  out.cameras.reserve(root.size());

  // Parse each supported camera record
  try {
    for (const Json& item : root) {
      if (!item.is_object()) {
        return StatusOr<CameraSet>::Error("invalid camera json");
      }

      const bool matrixCamera = item.contains("extrinsic") || item.contains("intrinsic");
      StatusOr<CameraParams> camera = matrixCamera ? ParseMatrixCamera(item) : ParseDirectXSplatCamera(item);
      if (!camera.ok()) {
        return StatusOr<CameraSet>::Error(camera.status.message);
      }
      out.cameras.push_back(std::move(camera.value));
    }
  } catch (const nlohmann::json::exception&) {
    return StatusOr<CameraSet>::Error("invalid camera json");
  }

  return StatusOr<CameraSet>::Ok(std::move(out));
}

}  // namespace dxsplat
