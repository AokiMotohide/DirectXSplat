#include "render/CameraFrameRenderer.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>

namespace dxsplat {

namespace {

using Microsoft::WRL::ComPtr;

constexpr size_t kCameraFrameVertexCount = 5;
constexpr size_t kCameraFrameIndexCount = 16;

struct CameraFrameInstance {
  float row0[4]{};
  float row1[4]{};
  float row2[4]{};
  float row3[4]{};
};

const char kCameraFrameShader[] = R"(
#pragma pack_matrix(row_major)

cbuffer FrameConstants : register(b0) {
  float4x4 viewProjection;
};

struct VSIn {
  float3 position : POSITION;
  float3 color : COLOR;
  float4 model0 : MODEL0;
  float4 model1 : MODEL1;
  float4 model2 : MODEL2;
  float4 model3 : MODEL3;
};

struct VSOut {
  float4 position : SV_Position;
  float4 color : COLOR;
};

VSOut VSMain(VSIn input) {
  float4x4 model = float4x4(input.model0, input.model1, input.model2, input.model3);
  float4 world = mul(model, float4(input.position, 1.0f));

  VSOut output;
  output.position = mul(viewProjection, world);
  output.color = float4(input.color, 1.0f);
  return output;
}

float4 PSMain(VSOut input) : SV_Target {
  return input.color;
}
)";

std::string HrString(HRESULT hr) {
  std::ostringstream ss;
  ss << "0x" << std::hex << static_cast<unsigned long>(hr);
  return ss.str();
}

Status CompileCameraFrameShader(const char* entry, const char* profile, ComPtr<ID3DBlob>& blob) {
  ComPtr<ID3DBlob> errorBlob;
  UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(NDEBUG)
  flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
  const HRESULT hr = D3DCompile(kCameraFrameShader,
                                std::strlen(kCameraFrameShader),
                                "camera_frame",
                                nullptr,
                                nullptr,
                                entry,
                                profile,
                                flags,
                                0,
                                blob.GetAddressOf(),
                                errorBlob.GetAddressOf());
  if (FAILED(hr)) {
    std::string message = std::string("camera frame shader compile failed: ") + entry;
    if (errorBlob != nullptr && errorBlob->GetBufferPointer() != nullptr && errorBlob->GetBufferSize() > 0) {
      message += " : ";
      message.append(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
    }
    return Status::Error(std::move(message));
  }
  return Status::Ok();
}

Vec3 TransformPoint(const Mat4& matrix, const Vec3& point) {
  const Vec4 localPoint{point.x, point.y, point.z, 1.0f};
  const Vec4 transformed = Mul(matrix, localPoint);
  if (std::abs(transformed.w) > 1e-6f) {
    const float invW = 1.0f / transformed.w;
    return {transformed.x * invW, transformed.y * invW, transformed.z * invW};
  }
  return {transformed.x, transformed.y, transformed.z};
}

Mat4 Mat4FromArray(const std::array<float, 16>& values) {
  Mat4 out{};
  out.m = values;
  return out;
}

Mat3 Mat3FromArray(const std::array<float, 9>& values) {
  Mat3 out{};
  out.m = values;
  return out;
}

Mat4 Scale4(float scale) {
  Mat4 out = Identity4();
  out.m[0] = scale;
  out.m[5] = scale;
  out.m[10] = scale;
  return out;
}

Mat4 Intrinsic4(const Mat3& intrinsic) {
  Mat4 out = Identity4();
  out.m = {
      intrinsic.m[0], intrinsic.m[1], intrinsic.m[2], 0.0f,
      intrinsic.m[3], intrinsic.m[4], intrinsic.m[5], 0.0f,
      intrinsic.m[6], intrinsic.m[7], intrinsic.m[8], 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f,
  };
  return out;
}

Mat4 NdcToImage(uint32_t width, uint32_t height) {
  const float w = static_cast<float>(std::max(width, 1u));
  const float h = static_cast<float>(std::max(height, 1u));
  Mat4 out = Identity4();
  out.m = {
      w * 0.5f, 0.0f,  w * 0.5f, 0.0f,
      0.0f,     h * 0.5f, h * 0.5f, 0.0f,
      0.0f,     0.0f,  1.0f,     0.0f,
      0.0f,     0.0f,  0.0f,     1.0f,
  };
  return out;
}

std::array<Vec3, kCameraFrameVertexCount> CanonicalCameraFrameVertices() {
  return {
      Vec3{0.0f, 0.0f, 0.0f},
      Vec3{-1.0f, -1.0f, 1.0f},
      Vec3{1.0f, -1.0f, 1.0f},
      Vec3{-1.0f, 1.0f, 1.0f},
      Vec3{1.0f, 1.0f, 1.0f},
  };
}

CameraFrameInstance InstanceModelFromMatrix(const Mat4& matrix) {
  CameraFrameInstance out{};
  std::memcpy(out.row0, matrix.m.data() + 0, sizeof(out.row0));
  std::memcpy(out.row1, matrix.m.data() + 4, sizeof(out.row1));
  std::memcpy(out.row2, matrix.m.data() + 8, sizeof(out.row2));
  std::memcpy(out.row3, matrix.m.data() + 12, sizeof(out.row3));
  return out;
}

}  // namespace

ViewerCamera ViewerCameraFromCameraParams(const CameraParams& camera) {
  ViewerCamera out{};
  out.name = camera.name;
  out.extrinsic = Mat4FromArray(camera.extrinsic);
  out.intrinsic = Mat3FromArray(camera.intrinsic);
  out.width = camera.width;
  out.height = camera.height;
  return out;
}

Mat4 BuildCameraFrameModelMatrix(const CameraParams& camera, float frameSize) {
  return BuildCameraFrameModelMatrix(ViewerCameraFromCameraParams(camera), frameSize);
}

Mat4 BuildCameraFrameModelMatrix(const ViewerCamera& camera, float frameSize) {
  const float safeFrameSize = std::max(frameSize, 0.0f);
  return Mul(Mul(Mul(Inverse(camera.extrinsic), Inverse(Intrinsic4(camera.intrinsic))),
                 NdcToImage(camera.width, camera.height)),
             Scale4(safeFrameSize));
}

std::array<uint32_t, kCameraFrameIndexCount> BuildCameraFrameIndices() {
  return {0, 1, 0, 2, 0, 3, 0, 4, 1, 2, 2, 4, 4, 3, 3, 1};
}

std::array<CameraFrameVertex, kCameraFrameVertexCount> BuildCameraFrameVertices(const CameraParams& camera,
                                                                                float frameSize) {
  return BuildCameraFrameVertices(ViewerCameraFromCameraParams(camera), frameSize);
}

std::array<CameraFrameVertex, kCameraFrameVertexCount> BuildCameraFrameVertices(const ViewerCamera& camera,
                                                                                float frameSize) {
  const Mat4 model = BuildCameraFrameModelMatrix(camera, frameSize);
  const auto canonical = CanonicalCameraFrameVertices();
  std::array<CameraFrameVertex, kCameraFrameVertexCount> out{};
  for (size_t i = 0; i < out.size(); ++i) {
    out[i].position = TransformPoint(model, canonical[i]);
  }
  return out;
}

Status CameraFrameRenderer::Initialize(ID3D12Device* device, DXGI_FORMAT colorFormat) {
  if (device == nullptr) {
    return Status::Error("invalid D3D12 device");
  }
  device_ = device;
  colorFormat_ = colorFormat;
  return CreatePipeline();
}

void CameraFrameRenderer::Shutdown() {
  for (FrameBuffers& frame : frames_) {
    ReleaseFrameBuffers(frame);
  }
  pipelineState_.Reset();
  rootSignature_.Reset();
  device_ = nullptr;
}

void CameraFrameRenderer::Invalidate() {
  for (FrameBuffers& frame : frames_) {
    frame.cachedCameraCount = 0;
    frame.cachedFrameSize = -1.0f;
    frame.instancesDirty = true;
  }
}

Status CameraFrameRenderer::CreatePipeline() {
  D3D12_ROOT_PARAMETER rootParameter{};
  rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  rootParameter.Constants.ShaderRegister = 0;
  rootParameter.Constants.RegisterSpace = 0;
  rootParameter.Constants.Num32BitValues = 16;
  rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

  ComPtr<ID3DBlob> rootBlob;
  ComPtr<ID3DBlob> errorBlob;
  D3D12_ROOT_SIGNATURE_DESC rootDesc{};
  rootDesc.NumParameters = 1;
  rootDesc.pParameters = &rootParameter;
  rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  HRESULT hr = D3D12SerializeRootSignature(&rootDesc,
                                           D3D_ROOT_SIGNATURE_VERSION_1,
                                           rootBlob.GetAddressOf(),
                                           errorBlob.GetAddressOf());
  if (FAILED(hr)) {
    return Status::Error("failed serializing camera frame root signature " + HrString(hr));
  }
  hr = device_->CreateRootSignature(0,
                                    rootBlob->GetBufferPointer(),
                                    rootBlob->GetBufferSize(),
                                    IID_PPV_ARGS(rootSignature_.GetAddressOf()));
  if (FAILED(hr)) {
    return Status::Error("failed creating camera frame root signature " + HrString(hr));
  }

  ComPtr<ID3DBlob> vsBlob;
  ComPtr<ID3DBlob> psBlob;
  Status status = CompileCameraFrameShader("VSMain", "vs_5_1", vsBlob);
  if (!status.ok) {
    return status;
  }
  status = CompileCameraFrameShader("PSMain", "ps_5_1", psBlob);
  if (!status.ok) {
    return status;
  }

  D3D12_INPUT_ELEMENT_DESC inputElements[6]{};
  inputElements[0] = {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
  inputElements[1] = {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
                      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
  inputElements[2] = {"MODEL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,
                      D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1};
  inputElements[3] = {"MODEL", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16,
                      D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1};
  inputElements[4] = {"MODEL", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32,
                      D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1};
  inputElements[5] = {"MODEL", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48,
                      D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1};

  D3D12_RASTERIZER_DESC raster{};
  raster.FillMode = D3D12_FILL_MODE_SOLID;
  raster.CullMode = D3D12_CULL_MODE_NONE;
  raster.DepthClipEnable = TRUE;

  D3D12_BLEND_DESC blend{};
  blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  D3D12_DEPTH_STENCIL_DESC depth{};
  depth.DepthEnable = FALSE;
  depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  depth.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  depth.StencilEnable = FALSE;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature_.Get();
  desc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
  desc.PS = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};
  desc.BlendState = blend;
  desc.SampleMask = UINT_MAX;
  desc.RasterizerState = raster;
  desc.DepthStencilState = depth;
  desc.InputLayout = {inputElements, 6};
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
  desc.NumRenderTargets = 1;
  desc.RTVFormats[0] = colorFormat_;
  desc.SampleDesc.Count = 1;

  hr = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(pipelineState_.GetAddressOf()));
  if (FAILED(hr)) {
    return Status::Error("failed creating camera frame pso " + HrString(hr));
  }
  return Status::Ok();
}

Status CameraFrameRenderer::EnsureUploadBuffer(size_t bytes,
                                               ComPtr<ID3D12Resource>& resource,
                                               void*& mapped,
                                               size_t& capacity) {
  bytes = std::max<size_t>(bytes, 1u);
  if (resource != nullptr && mapped != nullptr && capacity >= bytes) {
    return Status::Ok();
  }
  ReleaseUploadBuffer(resource, mapped, capacity);

  D3D12_HEAP_PROPERTIES heap{};
  heap.Type = D3D12_HEAP_TYPE_UPLOAD;
  heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heap.CreationNodeMask = 1;
  heap.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC desc{};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Width = bytes;
  desc.Height = 1;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.SampleDesc.Count = 1;
  desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  const HRESULT hr = device_->CreateCommittedResource(&heap,
                                                      D3D12_HEAP_FLAG_NONE,
                                                      &desc,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr,
                                                      IID_PPV_ARGS(resource.GetAddressOf()));
  if (FAILED(hr)) {
    return Status::Error("failed creating camera frame upload buffer " + HrString(hr));
  }

  D3D12_RANGE readRange{0, 0};
  void* mappedPtr = nullptr;
  const HRESULT mapHr = resource->Map(0, &readRange, &mappedPtr);
  if (FAILED(mapHr) || mappedPtr == nullptr) {
    resource.Reset();
    return Status::Error("failed mapping camera frame upload buffer " + HrString(mapHr));
  }

  mapped = mappedPtr;
  capacity = bytes;
  return Status::Ok();
}

void CameraFrameRenderer::ReleaseUploadBuffer(ComPtr<ID3D12Resource>& resource, void*& mapped, size_t& capacity) {
  if (resource != nullptr && mapped != nullptr) {
    resource->Unmap(0, nullptr);
  }
  resource.Reset();
  mapped = nullptr;
  capacity = 0;
}

void CameraFrameRenderer::ReleaseFrameBuffers(FrameBuffers& frame) {
  ReleaseUploadBuffer(frame.vertexBuffer, frame.mappedVertices, frame.vertexCapacityBytes);
  ReleaseUploadBuffer(frame.indexBuffer, frame.mappedIndices, frame.indexCapacityBytes);
  ReleaseUploadBuffer(frame.instanceBuffer, frame.mappedInstances, frame.instanceCapacityBytes);
  frame.cachedCameraCount = 0;
  frame.cachedFrameSize = -1.0f;
  frame.staticGeometryDirty = true;
  frame.instancesDirty = true;
}

Status CameraFrameRenderer::EnsureStaticGeometry(FrameBuffers& frame) {
  if (!frame.staticGeometryDirty && frame.vertexBuffer != nullptr && frame.indexBuffer != nullptr) {
    return Status::Ok();
  }

  const std::array<GpuVertex, kCameraFrameVertexCount> vertices = {{
      {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
      {{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
      {{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
      {{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
      {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
  }};
  const std::array<uint32_t, kCameraFrameIndexCount> indices = BuildCameraFrameIndices();

  Status status = EnsureUploadBuffer(vertices.size() * sizeof(GpuVertex),
                                     frame.vertexBuffer,
                                     frame.mappedVertices,
                                     frame.vertexCapacityBytes);
  if (!status.ok) {
    return status;
  }
  status = EnsureUploadBuffer(indices.size() * sizeof(uint32_t),
                              frame.indexBuffer,
                              frame.mappedIndices,
                              frame.indexCapacityBytes);
  if (!status.ok) {
    return status;
  }
  std::memcpy(frame.mappedVertices, vertices.data(), vertices.size() * sizeof(GpuVertex));
  std::memcpy(frame.mappedIndices, indices.data(), indices.size() * sizeof(uint32_t));
  frame.staticGeometryDirty = false;
  return Status::Ok();
}

Status CameraFrameRenderer::UpdateInstances(FrameBuffers& frame, const CameraSet& cameras, float frameSize) {
  const float safeFrameSize = std::max(frameSize, 0.0f);
  if (!frame.instancesDirty && frame.cachedCameraCount == cameras.cameras.size() &&
      std::abs(frame.cachedFrameSize - safeFrameSize) <= 1e-6f) {
    return Status::Ok();
  }

  std::vector<CameraFrameInstance> instances;
  instances.reserve(cameras.cameras.size());
  for (const CameraParams& camera : cameras.cameras) {
    instances.push_back(InstanceModelFromMatrix(BuildCameraFrameModelMatrix(camera, safeFrameSize)));
  }

  Status status =
      EnsureUploadBuffer(instances.size() * sizeof(CameraFrameInstance),
                         frame.instanceBuffer,
                         frame.mappedInstances,
                         frame.instanceCapacityBytes);
  if (!status.ok) {
    return status;
  }
  std::memcpy(frame.mappedInstances, instances.data(), instances.size() * sizeof(CameraFrameInstance));

  frame.cachedCameraCount = cameras.cameras.size();
  frame.cachedFrameSize = safeFrameSize;
  frame.instancesDirty = false;
  return Status::Ok();
}

Status CameraFrameRenderer::Render(ID3D12GraphicsCommandList* commandList,
                                   D3D12_CPU_DESCRIPTOR_HANDLE colorRtv,
                                   D3D12_VIEWPORT viewport,
                                   D3D12_RECT scissor,
                                   uint32_t frameSlot,
                                   const Mat4& view,
                                   const Mat4& projection,
                                   const CameraSet& cameras,
                                   const CameraUiState& uiState) {
  if (!uiState.showCameraFrames || cameras.cameras.empty()) {
    return Status::Ok();
  }
  if (device_ == nullptr || rootSignature_ == nullptr || pipelineState_ == nullptr) {
    return Status::Error("camera frame renderer is not initialized");
  }
  if (commandList == nullptr || colorRtv.ptr == 0) {
    return Status::Error("invalid camera frame render target");
  }

  FrameBuffers& frame = frames_[frameSlot % frames_.size()];

  Status status = EnsureStaticGeometry(frame);
  if (!status.ok) {
    return status;
  }
  status = UpdateInstances(frame, cameras, uiState.frameSize);
  if (!status.ok) {
    return status;
  }

  const Mat4 viewProjection = Mul(projection, view);

  commandList->OMSetRenderTargets(1, &colorRtv, FALSE, nullptr);
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetGraphicsRootSignature(rootSignature_.Get());
  commandList->SetGraphicsRoot32BitConstants(0, 16, viewProjection.m.data(), 0);
  commandList->SetPipelineState(pipelineState_.Get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

  D3D12_VERTEX_BUFFER_VIEW vbv[2]{};
  vbv[0].BufferLocation = frame.vertexBuffer->GetGPUVirtualAddress();
  vbv[0].SizeInBytes = static_cast<UINT>(kCameraFrameVertexCount * sizeof(GpuVertex));
  vbv[0].StrideInBytes = sizeof(GpuVertex);
  vbv[1].BufferLocation = frame.instanceBuffer->GetGPUVirtualAddress();
  vbv[1].SizeInBytes = static_cast<UINT>(cameras.cameras.size() * sizeof(CameraFrameInstance));
  vbv[1].StrideInBytes = sizeof(CameraFrameInstance);

  D3D12_INDEX_BUFFER_VIEW ibv{};
  ibv.BufferLocation = frame.indexBuffer->GetGPUVirtualAddress();
  ibv.SizeInBytes = static_cast<UINT>(kCameraFrameIndexCount * sizeof(uint32_t));
  ibv.Format = DXGI_FORMAT_R32_UINT;
  commandList->IASetVertexBuffers(0, 2, vbv);
  commandList->IASetIndexBuffer(&ibv);
  commandList->DrawIndexedInstanced(static_cast<UINT>(kCameraFrameIndexCount),
                                    static_cast<UINT>(cameras.cameras.size()),
                                    0,
                                    0,
                                    0);
  return Status::Ok();
}

}  // namespace dxsplat
