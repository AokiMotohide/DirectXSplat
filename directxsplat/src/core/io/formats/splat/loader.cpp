#include "io/formats/splat/loader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <new>
#include <stdexcept>
#include <vector>

#include "dxsplat/bounding.h"

namespace dxsplat::io {

namespace {

constexpr size_t kSplatRecordBytes = 32;
constexpr size_t kMaxSplatInputGaussians = 32ull * 1024ull * 1024ull;
constexpr uint64_t kMaxSplatExpandedBytes = 2ull * 1024ull * 1024ull * 1024ull;
constexpr size_t kMaxSplatExpandedGaussians = static_cast<size_t>(kMaxSplatExpandedBytes / sizeof(Gaussian));
constexpr size_t kMaxSplatGaussians =
    kMaxSplatExpandedGaussians < kMaxSplatInputGaussians ? kMaxSplatExpandedGaussians : kMaxSplatInputGaussians;
constexpr size_t kMaxSplatBytes = kMaxSplatGaussians * kSplatRecordBytes;
constexpr float kShC0 = 0.28209479177387814f;

StatusOr<std::vector<uint8_t>> ReadFileBytes(const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return StatusOr<std::vector<uint8_t>>::Error("failed to open splat file");
  }
  const std::streamoff end = file.tellg();
  if (end <= 0) {
    return StatusOr<std::vector<uint8_t>>::Error("empty splat file");
  }
  if (static_cast<uint64_t>(end) > kMaxSplatBytes) {
    return StatusOr<std::vector<uint8_t>>::Error("splat file is too large");
  }
  std::vector<uint8_t> bytes;
  try {
    bytes.resize(static_cast<size_t>(end));
  } catch (const std::bad_alloc&) {
    return StatusOr<std::vector<uint8_t>>::Error("splat file allocation failed");
  } catch (const std::length_error&) {
    return StatusOr<std::vector<uint8_t>>::Error("splat file is too large");
  }
  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!file) {
    return StatusOr<std::vector<uint8_t>>::Error("failed to read splat file");
  }
  return StatusOr<std::vector<uint8_t>>::Ok(std::move(bytes));
}

float ReadFloat32(const uint8_t* p) {
  uint32_t bits = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                  (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
  float out = 0.0f;
  std::memcpy(&out, &bits, sizeof(out));
  return out;
}

float DecodeUnorm8(uint8_t v) {
  return static_cast<float>(v) / 255.0f;
}

float DecodeQuatByte(uint8_t v) {
  return (static_cast<float>(v) - 128.0f) / 128.0f;
}

float SigmoidInv(float y) {
  const float e = std::clamp(y, 1e-6f, 1.0f - 1e-6f);
  return std::log(e / (1.0f - e));
}

Aabb ComputeGaussianBounds(const std::vector<Gaussian>& gaussians) {
  Aabb out{};
  if (gaussians.empty()) {
    return out;
  }
  out.min = gaussians[0].position;
  out.max = gaussians[0].position;
  out.valid = true;
  for (const Gaussian& g : gaussians) {
    out.min = Min(out.min, g.position);
    out.max = Max(out.max, g.position);
  }
  return out;
}

}

StatusOr<GaussianSet> SplatLoader::Load(const std::string& path, const std::string& setName) const try {
  const auto file = ReadFileBytes(path);
  if (!file.ok()) {
    return StatusOr<GaussianSet>::Error(file.status.message);
  }

  if ((file.value.size() % kSplatRecordBytes) != 0) {
    return StatusOr<GaussianSet>::Error("invalid splat file size");
  }

  const size_t count = file.value.size() / kSplatRecordBytes;
  if (count == 0) {
    return StatusOr<GaussianSet>::Error("splat scene has zero count");
  }
  if (count > kMaxSplatGaussians) {
    return StatusOr<GaussianSet>::Error("splat scene has too many splats");
  }

  GaussianSet set{};
  set.name = setName;
  set.gaussians.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    const uint8_t* row = file.value.data() + i * kSplatRecordBytes;
    Gaussian g{};
    g.splatId = static_cast<uint32_t>(i);
    g.position = {
        ReadFloat32(row + 0),
        ReadFloat32(row + 4),
        ReadFloat32(row + 8),
    };
    g.scale = {
        std::max(std::abs(ReadFloat32(row + 12)), 1e-6f),
        std::max(std::abs(ReadFloat32(row + 16)), 1e-6f),
        std::max(std::abs(ReadFloat32(row + 20)), 1e-6f),
    };
    g.sh[0] = (DecodeUnorm8(row[24]) - 0.5f) / kShC0;
    g.sh[16] = (DecodeUnorm8(row[25]) - 0.5f) / kShC0;
    g.sh[32] = (DecodeUnorm8(row[26]) - 0.5f) / kShC0;
    g.opacity = SigmoidInv(DecodeUnorm8(row[27]));
    g.rotation = Normalize({
        DecodeQuatByte(row[29]),
        DecodeQuatByte(row[30]),
        DecodeQuatByte(row[31]),
        DecodeQuatByte(row[28]),
    });

    if (!std::isfinite(g.position.x) || !std::isfinite(g.position.y) || !std::isfinite(g.position.z)) {
      continue;
    }
    if (!std::isfinite(g.scale.x) || !std::isfinite(g.scale.y) || !std::isfinite(g.scale.z)) {
      continue;
    }
    set.gaussians.push_back(g);
  }

  if (set.gaussians.empty()) {
    return StatusOr<GaussianSet>::Error("no valid gaussians found");
  }

  set.bounds = ComputeGaussianBounds(set.gaussians);
  return StatusOr<GaussianSet>::Ok(std::move(set));
} catch (const std::bad_alloc&) {
  return StatusOr<GaussianSet>::Error("splat scene allocation failed");
} catch (const std::length_error&) {
  return StatusOr<GaussianSet>::Error("splat scene allocation failed");
}

}

