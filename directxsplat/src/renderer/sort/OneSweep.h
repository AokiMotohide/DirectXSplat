#pragma once

#include <d3d12.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <wrl/client.h>

#include "dxsplat/status.h"
#include "GPUSorting/GPUSortingD3D12/GPUSorting.h"

namespace dxsplat {

struct OneSweepDispatch {
  ID3D12GraphicsCommandList* commandList = nullptr;
  ID3D12Resource* keyPrimaryResource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS keyPrimaryUav = 0;
  ID3D12Resource* keyTempResource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS keyTempUav = 0;
  ID3D12Resource* valuePrimaryResource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS valuePrimaryUav = 0;
  ID3D12Resource* valueTempResource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS valueTempUav = 0;
  ID3D12Resource* passHistogramResource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS passHistogramUav = 0;
  ID3D12Resource* globalHistogramResource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS globalHistogramUav = 0;
  ID3D12Resource* indexResource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS indexUav = 0;
  uint32_t elementCount = 0;
};

struct OneSweepIndirectDispatch {
  ID3D12GraphicsCommandList* commandList = nullptr;
  ID3D12Resource* keyPrimaryResource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS keyPrimaryUav = 0;
  ID3D12Resource* keyTempResource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS keyTempUav = 0;
  ID3D12Resource* valuePrimaryResource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS valuePrimaryUav = 0;
  ID3D12Resource* valueTempResource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS valueTempUav = 0;
  ID3D12Resource* passHistogramResource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS passHistogramUav = 0;
  ID3D12Resource* globalHistogramResource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS globalHistogramUav = 0;
  ID3D12Resource* indexResource = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS indexUav = 0;
  ID3D12Resource* argumentBufferResource = nullptr;
  uint64_t initArgsOffset = 0;
  uint64_t histogramArgsOffset = 0;
  uint64_t scanArgsOffset = 0;
  uint64_t digitArgsOffset = 0;
  uint32_t digitPassCount = 4;
};

struct OneSweepResult {
  bool sortedInPrimary = true;
  uint32_t passCount = 4;
};

class OneSweep {
 public:
  OneSweep();
  ~OneSweep();
  Status Initialize(ID3D12Device* device);
  void Shutdown();
  bool IsInitialized() const;
  uint32_t MaxPartitionsForElementCount(uint32_t elementCount) const;
  uint32_t PartitionSize() const;
  Status Dispatch(const OneSweepDispatch& dispatch, OneSweepResult& outResult) const;
  Status DispatchIndirect(const OneSweepIndirectDispatch& dispatch, OneSweepResult& outResult) const;

 private:
  struct Kernel;

  Status InitializeKernels();
  Status BuildCompileArguments();
  Status CompileKernel(const wchar_t* entryPoint,
                       const std::vector<CD3DX12_ROOT_PARAMETER1>& rootParameters,
                       std::unique_ptr<Kernel>& outKernel);
  Status QueryDeviceInfo();

  ID3D12Device* device_ = nullptr;
  GPUSorting::DeviceInfo deviceInfo_{};
  GPUSorting::TuningParameters tuning_{};
  std::vector<std::wstring> compileArguments_;
  std::unique_ptr<Kernel> initSweepKernel_;
  std::unique_ptr<Kernel> globalHistogramKernel_;
  std::unique_ptr<Kernel> scanKernel_;
  std::unique_ptr<Kernel> digitBinningKernel_;
};

}  // namespace dxsplat
