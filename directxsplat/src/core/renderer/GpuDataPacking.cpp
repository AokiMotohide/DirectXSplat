#include "renderer/GpuDataPacking.h"

#include <algorithm>
#include <cmath>

#include <DirectXPackedVector.h>

#include "common/Bounding.h"

namespace directxsplat {

namespace {

using DirectX::PackedVector::XMConvertFloatToHalf;

uint16_t FloatToHalfBitsDiag(float v) {
  return XMConvertFloatToHalf(v);
}

uint32_t PackHalf2Diag(float a, float b) {
  return static_cast<uint32_t>(FloatToHalfBitsDiag(a)) | (static_cast<uint32_t>(FloatToHalfBitsDiag(b)) << 16u);
}

uint16_t PackUnorm16Diag(float v) {
  const float clamped = Clamp(v, 0.0f, 1.0f);
  return static_cast<uint16_t>(std::lround(clamped * 65535.0f));
}

uint16_t PackSnorm16Diag(float v) {
  const float clamped = Clamp(v, -1.0f, 1.0f);
  const int scaled = static_cast<int>(std::lround(clamped * 32767.0f));
  return static_cast<uint16_t>(scaled & 0xFFFF);
}

uint32_t PackSnorm16PairDiag(float a, float b) {
  return static_cast<uint32_t>(PackSnorm16Diag(a)) | (static_cast<uint32_t>(PackSnorm16Diag(b)) << 16u);
}

Aabb BoundsFromGaussiansDiag(const GaussianSet& set) {
  std::vector<Vec3> points;
  points.reserve(set.gaussians.size());
  for (const Gaussian& g : set.gaussians) {
    points.push_back(g.position);
  }
  return ComputeAabb(points);
}

}  // namespace

PackedGaussianUpload PackGaussianUploadBuffers(const GaussianSet& set) {
  PackedGaussianUpload out{};
  out.positions.reserve(set.gaussians.size() * 3);
  out.scales.reserve(set.gaussians.size() * 3);
  out.rotations.reserve(set.gaussians.size() * 4);
  out.opacity.reserve(set.gaussians.size());
  out.sh.reserve(set.gaussians.size() * kShOrder3CoeffCountTotal);

  for (const auto& g : set.gaussians) {
    out.positions.push_back(g.position.x);
    out.positions.push_back(g.position.y);
    out.positions.push_back(g.position.z);

    out.scales.push_back(g.scale.x);
    out.scales.push_back(g.scale.y);
    out.scales.push_back(g.scale.z);

    out.rotations.push_back(g.rotation.x);
    out.rotations.push_back(g.rotation.y);
    out.rotations.push_back(g.rotation.z);
    out.rotations.push_back(g.rotation.w);

    out.opacity.push_back(g.opacity);
    out.sh.insert(out.sh.end(), g.sh.begin(), g.sh.end());
  }

  return out;
}

CompactGaussianUpload PackCompactGaussianUploadBuffers(const GaussianSet& set) {
  CompactGaussianUpload out{};
  const Aabb bounds = set.bounds.valid ? set.bounds : BoundsFromGaussiansDiag(set);
  const Vec3 decodeMin = bounds.valid ? bounds.min : Vec3{};
  const Vec3 decodeExtent = bounds.valid ? Max(bounds.max - bounds.min, Vec3{1e-6f, 1e-6f, 1e-6f})
                                         : Vec3{1.0f, 1.0f, 1.0f};
  out.decodeMin = {decodeMin.x, decodeMin.y, decodeMin.z};
  out.decodeExtent = {decodeExtent.x, decodeExtent.y, decodeExtent.z};
  out.strideBytes = 128u;
  out.words.reserve(set.gaussians.size() * 32u);

  for (const Gaussian& g : set.gaussians) {
    const Vec3 pos01{
        decodeExtent.x > 1e-8f ? (g.position.x - decodeMin.x) / decodeExtent.x : 0.0f,
        decodeExtent.y > 1e-8f ? (g.position.y - decodeMin.y) / decodeExtent.y : 0.0f,
        decodeExtent.z > 1e-8f ? (g.position.z - decodeMin.z) / decodeExtent.z : 0.0f,
    };
    const Vec3 scale{
        std::max(std::abs(g.scale.x), 1e-4f),
        std::max(std::abs(g.scale.y), 1e-4f),
        std::max(std::abs(g.scale.z), 1e-4f),
    };
    const float filterScale = std::max(std::min({scale.x, scale.y, scale.z}) * 0.18f, 1e-5f);
    const Quat q = Normalize(g.rotation);

    out.words.push_back(static_cast<uint32_t>(PackUnorm16Diag(pos01.x)) |
                        (static_cast<uint32_t>(PackUnorm16Diag(pos01.y)) << 16u));
    out.words.push_back(static_cast<uint32_t>(PackUnorm16Diag(pos01.z)) |
                        (static_cast<uint32_t>(FloatToHalfBitsDiag(g.opacity)) << 16u));
    out.words.push_back(PackHalf2Diag(scale.x, scale.y));
    out.words.push_back(PackHalf2Diag(scale.z, filterScale));
    out.words.push_back(PackSnorm16PairDiag(q.x, q.y));
    out.words.push_back(PackSnorm16PairDiag(q.z, q.w));
    for (size_t i = 0; i < kShOrder3CoeffCountTotal; i += 2u) {
      out.words.push_back(PackHalf2Diag(g.sh[i], g.sh[i + 1u]));
    }
    out.words.push_back(g.splatId);
    out.words.push_back(g.instanceId);
  }

  return out;
}

BindlessTable BuildBindlessTable(uint32_t resourceCount, uint32_t firstDescriptorIndex) {
  BindlessTable out{};
  out.descriptorIndices.resize(resourceCount);
  for (uint32_t i = 0; i < resourceCount; ++i) {
    out.descriptorIndices[i] = firstDescriptorIndex + i;
  }
  return out;
}

void RemapGlobalIndices(const std::vector<uint32_t>& localIndices, uint32_t instanceBase,
                        std::vector<uint32_t>& outGlobal) {
  outGlobal.resize(localIndices.size());
  for (size_t i = 0; i < localIndices.size(); ++i) {
    outGlobal[i] = instanceBase + localIndices[i];
  }
}

ChunkBookkeeping BuildChunkBookkeeping(uint32_t itemCount, uint32_t chunkSize) {
  ChunkBookkeeping out{};
  if (chunkSize == 0) {
    return out;
  }

  const uint32_t chunkCount = (itemCount + chunkSize - 1) / chunkSize;
  out.chunkStarts.resize(chunkCount);
  out.chunkCounts.resize(chunkCount);
  for (uint32_t i = 0; i < chunkCount; ++i) {
    const uint32_t start = i * chunkSize;
    const uint32_t count = std::min(chunkSize, itemCount - start);
    out.chunkStarts[i] = start;
    out.chunkCounts[i] = count;
  }
  return out;
}

Status ValidatePipelineParameterBlockLayout(size_t expectedSize, size_t expectedAlignment) {
  if (sizeof(PipelineParameterBlock) != expectedSize) {
    return Status::Error("PipelineParameterBlock size mismatch");
  }
  if (alignof(PipelineParameterBlock) < expectedAlignment) {
    return Status::Error("PipelineParameterBlock alignment mismatch");
  }
  return Status::Ok();
}

}  // namespace directxsplat
