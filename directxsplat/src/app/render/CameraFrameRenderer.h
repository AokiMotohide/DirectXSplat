#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <d3d12.h>
#include <dxgiformat.h>
#include <wrl/client.h>

#include "dxsplat/directxsplat.h"
#include "dxsplat/math.h"
#include "dxsplat/scene.h"
#include "dxsplat/status.h"
#include "ui/UiLayer.h"

namespace dxsplat {

struct CameraFrameVertex {
  Vec3 position{};
};

ViewerCamera ViewerCameraFromCameraParams(const CameraParams& camera);
Mat4 BuildCameraFrameModelMatrix(const ViewerCamera& camera, float frameSize);
Mat4 BuildCameraFrameModelMatrix(const CameraParams& camera, float frameSize);
std::array<CameraFrameVertex, 5> BuildCameraFrameVertices(const CameraParams& camera, float frameSize);
std::array<CameraFrameVertex, 5> BuildCameraFrameVertices(const ViewerCamera& camera, float frameSize);
std::array<uint32_t, 16> BuildCameraFrameIndices();

class CameraFrameRenderer {
 public:
  Status Initialize(ID3D12Device* device, DXGI_FORMAT colorFormat);
  void Shutdown();
  void Invalidate();

  Status Render(ID3D12GraphicsCommandList* commandList,
                D3D12_CPU_DESCRIPTOR_HANDLE colorRtv,
                D3D12_VIEWPORT viewport,
                D3D12_RECT scissor,
                const Mat4& view,
                const Mat4& projection,
                const CameraSet& cameras,
                const CameraUiState& uiState);

 private:
  struct GpuVertex {
    float position[3]{};
    float color[3]{};
  };

  Status CreatePipeline();
  Status EnsureStaticGeometry();
  Status UpdateInstances(const CameraSet& cameras, float frameSize);
  Status EnsureUploadBuffer(size_t bytes,
                            Microsoft::WRL::ComPtr<ID3D12Resource>& resource,
                            void*& mapped,
                            size_t& capacity);
  void ReleaseUploadBuffer(Microsoft::WRL::ComPtr<ID3D12Resource>& resource, void*& mapped, size_t& capacity);

  ID3D12Device* device_ = nullptr;
  DXGI_FORMAT colorFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
  Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
  Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
  Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_;
  void* mappedVertices_ = nullptr;
  void* mappedIndices_ = nullptr;
  void* mappedInstances_ = nullptr;
  size_t vertexCapacityBytes_ = 0;
  size_t indexCapacityBytes_ = 0;
  size_t instanceCapacityBytes_ = 0;
  size_t cachedCameraCount_ = 0;
  float cachedFrameSize_ = -1.0f;
  bool instancesDirty_ = true;
};

}  // namespace dxsplat
