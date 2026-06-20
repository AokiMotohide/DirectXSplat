#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "dxsplat/math.h"
#include "dxsplat/scene.h"
#include "dxsplat/sort.h"
#include "dxsplat/status.h"

namespace dxsplat {

struct PackedGaussianUpload {
  std::vector<float> positions;
  std::vector<float> scales;
  std::vector<float> rotations;
  std::vector<float> opacity;
  std::vector<float> sh;
};

struct CompactGaussianUpload {
  std::vector<uint32_t> words;
  std::array<float, 3> decodeMin{};
  std::array<float, 3> decodeExtent{};
  uint32_t strideBytes = 0;
};

struct BindlessTable {
  std::vector<uint32_t> descriptorIndices;
};

struct ChunkBookkeeping {
  std::vector<uint32_t> chunkStarts;
  std::vector<uint32_t> chunkCounts;
};

struct PipelineParameterBlock {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t flags = 0;
  float time = 0.0f;
  float scalingModifier = 1.0f;
  float pad[3]{};
};

PackedGaussianUpload PackGaussianUploadBuffers(const GaussianSet& set);
CompactGaussianUpload PackCompactGaussianUploadBuffers(const GaussianSet& set);
BindlessTable BuildBindlessTable(uint32_t resourceCount, uint32_t firstDescriptorIndex);
void RemapGlobalIndices(const std::vector<uint32_t>& localIndices, uint32_t instanceBase,
                        std::vector<uint32_t>& outGlobal);
ChunkBookkeeping BuildChunkBookkeeping(uint32_t itemCount, uint32_t chunkSize);
Status ValidatePipelineParameterBlockLayout(size_t expectedSize, size_t expectedAlignment);

struct FrustumPlane {
  Vec3 n{};
  float d = 0.0f;
};

using FrustumPlanes = std::array<FrustumPlane, 6>;

FrustumPlanes BuildFrustumPlanes(const Mat4& viewProj);
bool SphereInFrustum(const FrustumPlanes& planes, const Vec3& center, float radius);
void FrustumCull(const Scene& scene, const FrustumPlanes& planes, std::vector<uint32_t>& visibleGlobalIndices);

}  // namespace dxsplat
