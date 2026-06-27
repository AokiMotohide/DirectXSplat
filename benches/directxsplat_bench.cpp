#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "api/CameraSetInternal.h"
#include "api/GaussianSplatsInternal.h"
#include "directxsplat/directxsplat.h"
#include "directxsplat/gpu_resources.h"
#include "directxsplat/renderer.h"
#include "directxsplat/settings.h"

using Microsoft::WRL::ComPtr;

namespace {

constexpr uint32_t kFrameSlotCount = 3;
constexpr uint64_t kBytesPerPixel = 4;
constexpr DWORD kFenceWaitPollMs = 50;
constexpr const char* kHotRenderLabel = "resident-scene single-camera GPU render throughput";
constexpr const char* kQualityLabel = "quality/capture pass";
constexpr const char* kTimingScope =
    "Resident-scene, single-camera GPU render throughput. Scene loading, initial upload, warmup, CPU readback, UI, and presentation excluded. PSNR evaluated separately over the same camera set.";

enum class BenchMode {
  HotRender,
  Quality,
  Both,
};

struct BenchOptions {
  std::filesystem::path scenePath;
  std::filesystem::path cameraPath;
  std::filesystem::path outputDir;
  std::filesystem::path jsonPath;
  BenchMode mode = BenchMode::Both;
  uint32_t first = 0;
  uint32_t warmupSweeps = 2;
  uint32_t measurementSweeps = 5;
  uint64_t splatBudget = 0;
  float scaleModifier = 1.0f;
  bool fastCulling = true;
  directxsplat::DrawOptions draw{};
};

struct FrameRecord {
  std::string path;
};

struct TimingSummary {
  bool available = false;
  uint32_t cameraCount = 0;
  uint32_t measuredFrames = 0;
  uint32_t warmupSweeps = 0;
  uint32_t measurementSweeps = 0;
  double totalGpuMs = 0.0;
  double gpuFps = 0.0;
  double medianGpuMs = 0.0;
  double p95GpuMs = 0.0;
  double visibleMean = 0.0;
  double visibleP95 = 0.0;
};

struct QualitySummary {
  bool available = false;
  uint32_t frames = 0;
  std::vector<FrameRecord> images;
};

struct BenchSummary {
  uint64_t splats = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t cameraCount = 0;
  uint64_t residentGaussians = 0;
  uint32_t uploadedSceneCreateCount = 0;
  uint32_t uploadedSceneDestroyCount = 0;
  std::string adapterName;
  TimingSummary hot;
  QualitySummary quality;
};

struct FrameSlot {
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12GraphicsCommandList> commandList;
  ComPtr<ID3D12Resource> colorTarget;
  ComPtr<ID3D12Resource> colorReadback;
  D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  uint64_t readbackBytes = 0;
  D3D12_RESOURCE_STATES colorState = D3D12_RESOURCE_STATE_COMMON;
  uint64_t fenceValue = 0;
};

std::string HrString(HRESULT hr) {
  std::ostringstream ss;
  ss << "0x" << std::hex << static_cast<unsigned long>(hr);
  return ss.str();
}

directxsplat::Status HrStatus(HRESULT hr, const std::string& message) {
  return directxsplat::Status::Error(message + " " + HrString(hr));
}

std::string WideToUtf8(const WCHAR* value) {
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
  if (bytes <= 1) {
    return {};
  }
  std::string out(static_cast<size_t>(bytes - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), bytes, nullptr, nullptr);
  return out;
}

directxsplat::Status CheckDeviceRemoved(ID3D12Device* device) {
  if (device == nullptr) {
    return directxsplat::Status::Ok();
  }
  const HRESULT removed = device->GetDeviceRemovedReason();
  if (FAILED(removed)) {
    return HrStatus(removed, "D3D12 device removed");
  }
  return directxsplat::Status::Ok();
}

directxsplat::Status ValidateReadbackLayout(uint32_t width, uint32_t height, uint32_t rowPitch, uint64_t readbackBytes) {
  if (width == 0 || height == 0 || rowPitch == 0 || readbackBytes == 0) {
    return directxsplat::Status::Error("invalid readback layout");
  }
  const uint64_t rowBytes = static_cast<uint64_t>(width) * kBytesPerPixel;
  if (rowBytes > rowPitch) {
    return directxsplat::Status::Error("invalid readback layout");
  }
  if (height > std::numeric_limits<uint64_t>::max() / rowPitch || height > std::numeric_limits<uint64_t>::max() / rowBytes) {
    return directxsplat::Status::Error("image is too large");
  }
  const uint64_t requiredReadbackBytes = static_cast<uint64_t>(height - 1) * rowPitch + rowBytes;
  if (readbackBytes < requiredReadbackBytes) {
    return directxsplat::Status::Error("invalid readback layout");
  }
  return directxsplat::Status::Ok();
}

std::string JsonEscape(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (const char ch : value) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += static_cast<unsigned char>(ch) < 0x20 ? ' ' : ch;
        break;
    }
  }
  return out;
}

std::filesystem::path FramePath(const std::filesystem::path& dir, uint32_t index) {
  std::ostringstream name;
  name << std::setw(6) << std::setfill('0') << index << ".ppm";
  return dir / name.str();
}

double Percentile(std::vector<double> values, double percentile) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const double clamped = std::clamp(percentile, 0.0, 1.0);
  const size_t index = static_cast<size_t>(std::ceil(clamped * static_cast<double>(values.size())) - 1.0);
  return values[std::min(index, values.size() - 1)];
}

double Median(std::vector<double> values) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const size_t mid = values.size() / 2;
  if ((values.size() % 2) != 0) {
    return values[mid];
  }
  return (values[mid - 1] + values[mid]) * 0.5;
}

directxsplat::Status WritePpm(const std::filesystem::path& path, const directxsplat::ImageRgba8& image) {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return directxsplat::Status::Error("failed opening output image");
  }
  file << "P6\n" << image.width << " " << image.height << "\n255\n";
  const uint8_t* pixels = image.pixels.data();
  for (uint32_t i = 0; i < image.width * image.height; ++i) {
    file.put(static_cast<char>(pixels[i * 4 + 0]));
    file.put(static_cast<char>(pixels[i * 4 + 1]));
    file.put(static_cast<char>(pixels[i * 4 + 2]));
  }
  if (!file) {
    return directxsplat::Status::Error("failed writing output image");
  }
  return directxsplat::Status::Ok();
}

void WriteImagesJson(std::ofstream& file, const std::vector<FrameRecord>& images, int indent) {
  const std::string pad(static_cast<size_t>(indent), ' ');
  file << pad << "\"images\": [\n";
  for (size_t i = 0; i < images.size(); ++i) {
    file << pad << "  \"" << JsonEscape(images[i].path) << "\"";
    file << (i + 1 == images.size() ? "\n" : ",\n");
  }
  file << pad << "]";
}

directxsplat::Status WriteSummaryJson(const std::filesystem::path& path, const BenchSummary& summary) {
  std::ofstream file(path);
  if (!file) {
    return directxsplat::Status::Error("failed opening summary json");
  }
  file << "{\n";
  file << "  \"implementation\": \"DirectXSplat\",\n";
  file << "  \"timing_scope\": \"" << JsonEscape(kTimingScope) << "\",\n";
  file << "  \"splats\": " << summary.splats << ",\n";
  file << "  \"width\": " << summary.width << ",\n";
  file << "  \"height\": " << summary.height << ",\n";
  file << "  \"frames\": " << summary.cameraCount << ",\n";
  file << "  \"camera_count\": " << summary.cameraCount << ",\n";
  file << "  \"resident_gaussians\": " << summary.residentGaussians << ",\n";
  file << "  \"adapter_name\": \"" << JsonEscape(summary.adapterName) << "\",\n";
  file << "  \"uploaded_scene_create_count\": " << summary.uploadedSceneCreateCount << ",\n";
  file << "  \"uploaded_scene_destroy_count\": " << summary.uploadedSceneDestroyCount << ",\n";
  file << "  \"hot_render\": ";
  if (summary.hot.available) {
    file << "{\n";
    file << "    \"label\": \"" << kHotRenderLabel << "\",\n";
    file << "    \"frames\": " << summary.hot.measuredFrames << ",\n";
    file << "    \"camera_count\": " << summary.hot.cameraCount << ",\n";
    file << "    \"warmup_sweeps\": " << summary.hot.warmupSweeps << ",\n";
    file << "    \"measurement_sweeps\": " << summary.hot.measurementSweeps << ",\n";
    file << "    \"total_gpu_ms\": " << std::fixed << std::setprecision(6) << summary.hot.totalGpuMs << ",\n";
    file << "    \"gpu_fps\": " << std::fixed << std::setprecision(3) << summary.hot.gpuFps << ",\n";
    file << "    \"median_gpu_ms\": " << std::fixed << std::setprecision(6) << summary.hot.medianGpuMs << ",\n";
    file << "    \"p95_gpu_ms\": " << std::fixed << std::setprecision(6) << summary.hot.p95GpuMs << ",\n";
    file << "    \"visible_splats_mean\": " << std::fixed << std::setprecision(3) << summary.hot.visibleMean << ",\n";
    file << "    \"visible_splats_p95\": " << std::fixed << std::setprecision(3) << summary.hot.visibleP95 << "\n";
    file << "  },\n";
  } else {
    file << "null,\n";
  }
  file << "  \"quality\": ";
  if (summary.quality.available) {
    file << "{\n";
    file << "    \"label\": \"" << kQualityLabel << "\",\n";
    file << "    \"frames\": " << summary.quality.frames << ",\n";
    WriteImagesJson(file, summary.quality.images, 4);
    file << "\n  }\n";
  } else {
    file << "null\n";
  }
  file << "}\n";
  if (!file) {
    return directxsplat::Status::Error("failed writing summary json");
  }
  return directxsplat::Status::Ok();
}

directxsplat::RenderType ParseRenderType(std::string_view value) {
  if (value == "alpha") {
    return directxsplat::RenderType::Alpha;
  }
  if (value == "depth") {
    return directxsplat::RenderType::Depth;
  }
  return directxsplat::RenderType::Color;
}

BenchMode ParseBenchMode(std::string_view value) {
  if (value == "hot-render") {
    return BenchMode::HotRender;
  }
  if (value == "quality") {
    return BenchMode::Quality;
  }
  return BenchMode::Both;
}

directxsplat::ShadingDegree ParseShadingDegree(uint32_t value) {
  switch (value) {
    case 0:
      return directxsplat::ShadingDegree::Dc;
    case 1:
      return directxsplat::ShadingDegree::Degree1;
    case 2:
      return directxsplat::ShadingDegree::Degree2;
    default:
      return directxsplat::ShadingDegree::Degree3;
  }
}

void PrintUsage() {
  std::cout
      << "Usage: DirectXSplatBench --scene scene.ply --cameras cameras.json [options]\n"
      << "Options:\n"
      << "  --mode hot-render|quality|both\n"
      << "  --out DIR                    Write quality-pass PPM frames\n"
      << "  --json FILE                  Write JSON summary\n"
      << "  --first N                    Render first N cameras\n"
      << "  --warmup-sweeps N            Complete camera sweeps before timing\n"
      << "  --measurement-sweeps N       Complete camera sweeps for hot-render timing\n"
      << "  --near VALUE                 Near plane\n"
      << "  --far VALUE                  Far plane\n"
      << "  --render-type color|alpha|depth\n"
      << "  --sh-degree 0|1|2|3\n"
      << "  --no-aa\n"
      << "  --gamma\n";
}

directxsplat::StatusOr<BenchOptions> ParseArgs(int argc, char** argv) {
  BenchOptions options{};
  options.draw.nearPlane = 0.1f;
  options.draw.farPlane = 1000.0f;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto needValue = [&](const char* name) -> directxsplat::StatusOr<std::string> {
      if (i + 1 >= argc) {
        return directxsplat::StatusOr<std::string>::Error(std::string("missing value for ") + name);
      }
      ++i;
      return directxsplat::StatusOr<std::string>::Ok(argv[i]);
    };

    if (arg == "--help" || arg == "-h") {
      PrintUsage();
      std::exit(0);
    } else if (arg == "--scene") {
      auto value = needValue("--scene");
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
      options.scenePath = value.value;
    } else if (arg == "--cameras") {
      auto value = needValue("--cameras");
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
      options.cameraPath = value.value;
    } else if (arg == "--mode") {
      auto value = needValue("--mode");
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
      options.mode = ParseBenchMode(value.value);
    } else if (arg == "--out") {
      auto value = needValue("--out");
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
      options.outputDir = value.value;
    } else if (arg == "--json") {
      auto value = needValue("--json");
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
      options.jsonPath = value.value;
    } else if (arg == "--first") {
      auto value = needValue("--first");
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
      options.first = static_cast<uint32_t>(std::stoul(value.value));
    } else if (arg == "--warmup-sweeps" || arg == "--warmup") {
      auto value = needValue(arg.c_str());
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
      options.warmupSweeps = std::max<uint32_t>(2, static_cast<uint32_t>(std::stoul(value.value)));
    } else if (arg == "--measurement-sweeps") {
      auto value = needValue("--measurement-sweeps");
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
      options.measurementSweeps = std::max<uint32_t>(1, static_cast<uint32_t>(std::stoul(value.value)));
    } else if (arg == "--upload-warmup-limit") {
      auto value = needValue("--upload-warmup-limit");
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
    } else if (arg == "--near") {
      auto value = needValue("--near");
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
      options.draw.nearPlane = std::stof(value.value);
    } else if (arg == "--far") {
      auto value = needValue("--far");
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
      options.draw.farPlane = std::stof(value.value);
    } else if (arg == "--render-type") {
      auto value = needValue("--render-type");
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
      options.draw.renderType = ParseRenderType(value.value);
    } else if (arg == "--sh-degree") {
      auto value = needValue("--sh-degree");
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
      options.draw.shadingDegree = ParseShadingDegree(static_cast<uint32_t>(std::stoul(value.value)));
    } else if (arg == "--splat-budget") {
      auto value = needValue("--splat-budget");
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
      options.splatBudget = std::stoull(value.value);
    } else if (arg == "--scale-modifier") {
      auto value = needValue("--scale-modifier");
      if (!value.ok()) return directxsplat::StatusOr<BenchOptions>::Error(value.status.message);
      options.scaleModifier = std::stof(value.value);
    } else if (arg == "--no-aa") {
      options.draw.antialiasing = false;
    } else if (arg == "--gamma") {
      options.draw.gammaCorrection = true;
    } else if (arg == "--no-fast-culling") {
      options.fastCulling = false;
    } else {
      return directxsplat::StatusOr<BenchOptions>::Error("unknown argument " + arg);
    }
  }

  if (options.scenePath.empty()) {
    return directxsplat::StatusOr<BenchOptions>::Error("scene path is required");
  }
  if (options.cameraPath.empty()) {
    return directxsplat::StatusOr<BenchOptions>::Error("camera path is required");
  }
  return directxsplat::StatusOr<BenchOptions>::Ok(std::move(options));
}

class BenchRuntime {
 public:
  ~BenchRuntime() {
    renderer_.Shutdown();
    context_.Shutdown();
    if (fenceEvent_ != nullptr) {
      CloseHandle(fenceEvent_);
      fenceEvent_ = nullptr;
    }
  }

  directxsplat::Status Initialize() {
    if (initialized_) {
      return directxsplat::Status::Ok();
    }

    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(factory_.GetAddressOf()));
    if (FAILED(hr)) {
      ComPtr<IDXGIFactory1> factory1;
      hr = CreateDXGIFactory1(IID_PPV_ARGS(factory1.GetAddressOf()));
      if (SUCCEEDED(hr)) {
        hr = factory1.As(&factory_);
      }
    }
    if (FAILED(hr)) {
      return HrStatus(hr, "failed creating DXGI factory");
    }

    for (UINT index = 0;; ++index) {
      ComPtr<IDXGIAdapter1> candidate;
      hr = factory_->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(candidate.GetAddressOf()));
      if (hr == DXGI_ERROR_NOT_FOUND) {
        break;
      }
      if (FAILED(hr)) {
        break;
      }
      DXGI_ADAPTER_DESC1 desc{};
      candidate->GetDesc1(&desc);
      if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
        continue;
      }
      if (SUCCEEDED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device_.ReleaseAndGetAddressOf())))) {
        adapter_ = candidate;
        adapterName_ = WideToUtf8(desc.Description);
        break;
      }
    }

    if (device_ == nullptr) {
      hr = factory_->EnumWarpAdapter(IID_PPV_ARGS(adapter_.ReleaseAndGetAddressOf()));
      if (FAILED(hr)) {
        return HrStatus(hr, "failed acquiring WARP adapter");
      }
      hr = D3D12CreateDevice(adapter_.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device_.ReleaseAndGetAddressOf()));
      if (FAILED(hr)) {
        return HrStatus(hr, "failed creating D3D12 device");
      }
      DXGI_ADAPTER_DESC1 desc{};
      adapter_->GetDesc1(&desc);
      adapterName_ = WideToUtf8(desc.Description);
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(queue_.GetAddressOf()));
    if (FAILED(hr)) {
      return HrStatus(hr, "failed creating command queue");
    }
    if (FAILED(queue_->GetTimestampFrequency(&timestampFrequency_)) || timestampFrequency_ == 0) {
      return directxsplat::Status::Error("failed querying timestamp frequency");
    }

    for (FrameSlot& slot : slots_) {
      hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(slot.allocator.GetAddressOf()));
      if (FAILED(hr)) {
        return HrStatus(hr, "failed creating command allocator");
      }
      hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, slot.allocator.Get(), nullptr,
                                      IID_PPV_ARGS(slot.commandList.GetAddressOf()));
      if (FAILED(hr)) {
        return HrStatus(hr, "failed creating command list");
      }
      hr = slot.commandList->Close();
      if (FAILED(hr)) {
        return HrStatus(hr, "failed closing command list");
      }
    }

    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(resolveAllocator_.GetAddressOf()));
    if (FAILED(hr)) {
      return HrStatus(hr, "failed creating resolve command allocator");
    }
    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, resolveAllocator_.Get(), nullptr,
                                    IID_PPV_ARGS(resolveList_.GetAddressOf()));
    if (FAILED(hr)) {
      return HrStatus(hr, "failed creating resolve command list");
    }
    hr = resolveList_->Close();
    if (FAILED(hr)) {
      return HrStatus(hr, "failed closing resolve command list");
    }

    hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
    if (FAILED(hr)) {
      return HrStatus(hr, "failed creating fence");
    }
    fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent_ == nullptr) {
      return directxsplat::Status::Error("failed creating fence event");
    }

    directxsplat::Status contextStatus = context_.Initialize(device_.Get(), queue_.Get(), fence_.Get());
    if (!contextStatus.ok) {
      return contextStatus;
    }
    directxsplat::Status rendererStatus = renderer_.Initialize(context_);
    if (!rendererStatus.ok) {
      return rendererStatus;
    }

    initialized_ = true;
    return directxsplat::Status::Ok();
  }

  directxsplat::Renderer& Renderer() { return renderer_; }
  const std::string& AdapterName() const { return adapterName_; }

  directxsplat::Status EnsureTargets(uint32_t width, uint32_t height) {
    if (targetsReady_ && width_ == width && height_ == height) {
      return directxsplat::Status::Ok();
    }

    for (FrameSlot& slot : slots_) {
      directxsplat::Status waitStatus = WaitForSlot(slot);
      if (!waitStatus.ok) {
        return waitStatus;
      }
      slot.colorTarget.Reset();
      slot.colorReadback.Reset();
      slot.colorState = D3D12_RESOURCE_STATE_COMMON;
      slot.footprint = {};
      slot.readbackBytes = 0;
    }
    rtvHeap_.Reset();
    width_ = width;
    height_ = height;
    targetsReady_ = false;

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = kFrameSlotCount;
    HRESULT hr = device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHeap_.GetAddressOf()));
    if (FAILED(hr)) {
      return HrStatus(hr, "failed creating RTV heap");
    }
    rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    UINT rowCount = 0;
    UINT64 rowBytes = 0;
    UINT64 totalBytes = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    device_->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &rowCount, &rowBytes, &totalBytes);
    directxsplat::Status layoutStatus = ValidateReadbackLayout(width, height, footprint.Footprint.RowPitch, totalBytes);
    if (!layoutStatus.ok) {
      return layoutStatus;
    }

    for (uint32_t i = 0; i < kFrameSlotCount; ++i) {
      FrameSlot& slot = slots_[i];
      D3D12_HEAP_PROPERTIES heapProps{};
      heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      D3D12_CLEAR_VALUE clearValue{};
      clearValue.Format = desc.Format;
      clearValue.Color[3] = 1.0f;

      hr = device_->CreateCommittedResource(&heapProps,
                                            D3D12_HEAP_FLAG_NONE,
                                            &desc,
                                            D3D12_RESOURCE_STATE_COMMON,
                                            &clearValue,
                                            IID_PPV_ARGS(slot.colorTarget.GetAddressOf()));
      if (FAILED(hr)) {
        return HrStatus(hr, "failed creating color target");
      }

      slot.rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
      slot.rtv.ptr += static_cast<SIZE_T>(i) * rtvDescriptorSize_;
      device_->CreateRenderTargetView(slot.colorTarget.Get(), nullptr, slot.rtv);

      D3D12_HEAP_PROPERTIES readbackHeapProps{};
      readbackHeapProps.Type = D3D12_HEAP_TYPE_READBACK;
      readbackHeapProps.CreationNodeMask = 1;
      readbackHeapProps.VisibleNodeMask = 1;

      D3D12_RESOURCE_DESC readbackDesc{};
      readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      readbackDesc.Width = std::max<UINT64>(totalBytes, 4ull);
      readbackDesc.Height = 1;
      readbackDesc.DepthOrArraySize = 1;
      readbackDesc.MipLevels = 1;
      readbackDesc.SampleDesc.Count = 1;
      readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

      hr = device_->CreateCommittedResource(&readbackHeapProps,
                                            D3D12_HEAP_FLAG_NONE,
                                            &readbackDesc,
                                            D3D12_RESOURCE_STATE_COPY_DEST,
                                            nullptr,
                                            IID_PPV_ARGS(slot.colorReadback.GetAddressOf()));
      if (FAILED(hr)) {
        return HrStatus(hr, "failed creating readback buffer");
      }

      slot.footprint = footprint;
      slot.readbackBytes = totalBytes;
      slot.colorState = D3D12_RESOURCE_STATE_COMMON;
    }

    targetsReady_ = true;
    return directxsplat::Status::Ok();
  }

  directxsplat::Status RunHotRender(directxsplat::UploadedSceneHandle sceneHandle,
                                    const std::vector<directxsplat::CameraParams>& cameras,
                                    const BenchOptions& options,
                                    TimingSummary& summary) {
    if (cameras.empty()) {
      return directxsplat::Status::Error("camera set is empty");
    }
    const uint32_t measuredFrames = static_cast<uint32_t>(cameras.size()) * options.measurementSweeps;
    directxsplat::Status targetStatus = EnsureTargets(cameras.front().width, cameras.front().height);
    if (!targetStatus.ok) {
      return targetStatus;
    }

    directxsplat::Status warmupStatus = RunHotSweeps(sceneHandle, cameras, options, options.warmupSweeps, false, 0, nullptr, nullptr);
    if (!warmupStatus.ok) {
      return warmupStatus;
    }
    directxsplat::Status waitStatus = WaitForAllSlots();
    if (!waitStatus.ok) {
      return waitStatus;
    }

    directxsplat::Status queryStatus = EnsureTimestampResources(measuredFrames * 2u);
    if (!queryStatus.ok) {
      return queryStatus;
    }

    std::vector<double> visibleValues;
    visibleValues.reserve(measuredFrames);
    directxsplat::Status renderStatus =
        RunHotSweeps(sceneHandle, cameras, options, options.measurementSweeps, true, 0, &visibleValues, nullptr);
    if (!renderStatus.ok) {
      return renderStatus;
    }

    std::vector<double> gpuTimes;
    directxsplat::Status resolveStatus = ResolveMeasuredTimestamps(measuredFrames * 2u, gpuTimes);
    if (!resolveStatus.ok) {
      return resolveStatus;
    }

    double totalGpuMs = 0.0;
    for (double value : gpuTimes) {
      totalGpuMs += value;
    }

    summary.available = true;
    summary.cameraCount = static_cast<uint32_t>(cameras.size());
    summary.measuredFrames = measuredFrames;
    summary.warmupSweeps = options.warmupSweeps;
    summary.measurementSweeps = options.measurementSweeps;
    summary.totalGpuMs = totalGpuMs;
    summary.gpuFps = totalGpuMs > 0.0 ? static_cast<double>(measuredFrames) * 1000.0 / totalGpuMs : 0.0;
    summary.medianGpuMs = Median(gpuTimes);
    summary.p95GpuMs = Percentile(gpuTimes, 0.95);
    if (!visibleValues.empty()) {
      double visibleSum = 0.0;
      for (double value : visibleValues) {
        visibleSum += value;
      }
      summary.visibleMean = visibleSum / static_cast<double>(visibleValues.size());
      summary.visibleP95 = Percentile(visibleValues, 0.95);
    }
    return directxsplat::Status::Ok();
  }

  directxsplat::Status RunQuality(directxsplat::UploadedSceneHandle sceneHandle,
                                  const std::vector<directxsplat::CameraParams>& cameras,
                                  const BenchOptions& options,
                                  QualitySummary& summary) {
    if (cameras.empty()) {
      return directxsplat::Status::Error("camera set is empty");
    }
    directxsplat::Status targetStatus = EnsureTargets(cameras.front().width, cameras.front().height);
    if (!targetStatus.ok) {
      return targetStatus;
    }

    summary.available = true;
    summary.frames = static_cast<uint32_t>(cameras.size());
    for (uint32_t i = 0; i < summary.frames; ++i) {
      FrameSlot& slot = slots_[i % kFrameSlotCount];
      directxsplat::ImageRgba8 image{};
      directxsplat::FrameStats stats{};
      directxsplat::Status frameStatus = RenderQualityFrame(sceneHandle, cameras[i], options, slot, &image, &stats);
      if (!frameStatus.ok) {
        return frameStatus;
      }

      if (!options.outputDir.empty()) {
        const std::filesystem::path framePath = FramePath(options.outputDir, i);
        directxsplat::Status writeStatus = WritePpm(framePath, image);
        if (!writeStatus.ok) {
          return writeStatus;
        }
        summary.images.push_back({framePath.string()});
      }
    }
    return directxsplat::Status::Ok();
  }

 private:
  directxsplat::Status RunHotSweeps(directxsplat::UploadedSceneHandle sceneHandle,
                                    const std::vector<directxsplat::CameraParams>& cameras,
                                    const BenchOptions& options,
                                    uint32_t sweeps,
                                    bool measured,
                                    uint32_t firstQuery,
                                    std::vector<double>* visibleValues,
                                    std::vector<directxsplat::FrameStats>* statsValues) {
    uint32_t submitted = 0;
    for (uint32_t sweep = 0; sweep < sweeps; ++sweep) {
      for (const directxsplat::CameraParams& camera : cameras) {
        FrameSlot& slot = slots_[submitted % kFrameSlotCount];
        const uint32_t queryIndex = firstQuery + submitted * 2u;
        directxsplat::FrameStats stats{};
        directxsplat::Status frameStatus = RenderHotFrame(sceneHandle, camera, options, slot, measured, queryIndex, &stats);
        if (!frameStatus.ok) {
          return frameStatus;
        }
        if (visibleValues != nullptr) {
          visibleValues->push_back(static_cast<double>(stats.gaussiansVisible));
        }
        if (statsValues != nullptr) {
          statsValues->push_back(stats);
        }
        ++submitted;
      }
    }
    return directxsplat::Status::Ok();
  }

  directxsplat::Status RenderHotFrame(directxsplat::UploadedSceneHandle sceneHandle,
                                      const directxsplat::CameraParams& camera,
                                      const BenchOptions& options,
                                      FrameSlot& slot,
                                      bool measured,
                                      uint32_t queryIndex,
                                      directxsplat::FrameStats* outStats) {
    directxsplat::Status waitStatus = WaitForSlot(slot);
    if (!waitStatus.ok) {
      return waitStatus;
    }
    directxsplat::RenderFrameContext frameContext = FrameContext();

    auto input = BuildRenderInput(camera, options, frameContext.frameIndex);
    if (!input.ok()) {
      return input.status;
    }

    directxsplat::RenderPreparationResult preparation{};
    directxsplat::Status prepareStatus = renderer_.PrepareSceneForRender(sceneHandle, input.value, frameContext, &preparation);
    if (!prepareStatus.ok) {
      return prepareStatus;
    }

    directxsplat::Status resetStatus = ResetSlot(slot);
    if (!resetStatus.ok) {
      return resetStatus;
    }

    if (measured) {
      slot.commandList->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryIndex);
    }

    directxsplat::RenderResult renderResult{};
    directxsplat::RenderTargetBinding target = TargetBinding(slot, options.draw, camera.width, camera.height, false);
    directxsplat::Status renderStatus = renderer_.Render(slot.commandList.Get(), target, sceneHandle, input.value, frameContext, renderResult);
    if (renderStatus.ok || renderResult.submission.submissionRequired) {
      slot.colorState = target.colorStateAfter;
    }

    if (measured) {
      slot.commandList->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryIndex + 1u);
    }

    directxsplat::Status submitStatus = SubmitSlot(slot, renderResult.submission.uploadSyncPoint);
    if (!renderStatus.ok) {
      return renderStatus;
    }
    if (!submitStatus.ok) {
      return submitStatus;
    }
    if (outStats != nullptr) {
      *outStats = renderResult.stats.gaussiansTotal != 0 ? renderResult.stats : preparation.stats;
    }
    return directxsplat::Status::Ok();
  }

  directxsplat::Status RenderQualityFrame(directxsplat::UploadedSceneHandle sceneHandle,
                                          const directxsplat::CameraParams& camera,
                                          const BenchOptions& options,
                                          FrameSlot& slot,
                                          directxsplat::ImageRgba8* outImage,
                                          directxsplat::FrameStats* outStats) {
    directxsplat::Status waitStatus = WaitForSlot(slot);
    if (!waitStatus.ok) {
      return waitStatus;
    }
    directxsplat::RenderFrameContext frameContext = FrameContext();

    auto input = BuildRenderInput(camera, options, frameContext.frameIndex);
    if (!input.ok()) {
      return input.status;
    }

    directxsplat::RenderPreparationResult preparation{};
    directxsplat::Status prepareStatus = renderer_.PrepareSceneForRender(sceneHandle, input.value, frameContext, &preparation);
    if (!prepareStatus.ok) {
      return prepareStatus;
    }

    directxsplat::Status resetStatus = ResetSlot(slot);
    if (!resetStatus.ok) {
      return resetStatus;
    }

    directxsplat::RenderResult renderResult{};
    directxsplat::RenderTargetBinding target = TargetBinding(slot, options.draw, camera.width, camera.height, true);
    directxsplat::Status renderStatus = renderer_.Render(slot.commandList.Get(), target, sceneHandle, input.value, frameContext, renderResult);
    if (renderStatus.ok || renderResult.submission.submissionRequired) {
      slot.colorState = target.colorStateAfter;
    }
    if (renderStatus.ok) {
      QueueColorReadback(slot);
    }

    directxsplat::Status submitStatus = SubmitSlot(slot, renderResult.submission.uploadSyncPoint);
    if (!renderStatus.ok) {
      return renderStatus;
    }
    if (!submitStatus.ok) {
      return submitStatus;
    }

    waitStatus = WaitForSlot(slot);
    if (!waitStatus.ok) {
      return waitStatus;
    }
    if (outStats != nullptr) {
      *outStats = renderResult.stats.gaussiansTotal != 0 ? renderResult.stats : preparation.stats;
    }
    if (outImage != nullptr) {
      auto image = ReadbackImage(slot);
      if (!image.ok()) {
        return image.status;
      }
      *outImage = std::move(image.value);
    }
    return directxsplat::Status::Ok();
  }

  directxsplat::StatusOr<directxsplat::RenderInput> BuildRenderInput(const directxsplat::CameraParams& camera,
                                                                     const BenchOptions& options,
                                                                     uint64_t frameIndex) const {
    directxsplat::Status cameraStatus = directxsplat::ValidateCameraParamsForRendering(camera);
    if (!cameraStatus.ok) {
      return directxsplat::StatusOr<directxsplat::RenderInput>::Error(cameraStatus.message);
    }

    const directxsplat::CameraRenderState cameraState =
        directxsplat::CameraRenderStateFromCameraParams(camera, options.draw.nearPlane, options.draw.farPlane);

    directxsplat::RenderInput input{};
    input.view = cameraState.view;
    input.proj = cameraState.proj;
    input.cameraPosition = cameraState.position;
    input.viewportWidth = camera.width;
    input.viewportHeight = camera.height;
    input.nearPlane = options.draw.nearPlane;
    input.farPlane = options.draw.farPlane;
    input.settings.antialiasing = options.draw.antialiasing;
    input.settings.antialiasingStrength = options.draw.antialiasingStrength;
    input.settings.backgroundColor = {options.draw.background[0], options.draw.background[1], options.draw.background[2]};
    input.settings.fastCulling = options.fastCulling;
    input.settings.gammaCorrection = options.draw.gammaCorrection;
    input.settings.gaussianScalingModifier = options.scaleModifier;
    input.settings.renderType = options.draw.renderType;
    input.settings.shadingDegree = options.draw.shadingDegree;
    input.settings.outputDepth = options.draw.renderType == directxsplat::RenderType::Depth;
    input.settings.splatBudget = options.splatBudget;
    input.frameIndex = frameIndex;
    return directxsplat::StatusOr<directxsplat::RenderInput>::Ok(input);
  }

  directxsplat::RenderFrameContext FrameContext() {
    directxsplat::RenderFrameContext out{};
    out.fence = fence_.Get();
    out.completedFenceValue = fence_ != nullptr ? fence_->GetCompletedValue() : 0;
    out.submissionFenceValue = fenceValue_ + 1;
    out.frameIndex = frameIndex_++;
    return out;
  }

  directxsplat::RenderTargetBinding TargetBinding(const FrameSlot& slot,
                                                  const directxsplat::DrawOptions& options,
                                                  uint32_t width,
                                                  uint32_t height,
                                                  bool readback) const {
    directxsplat::RenderTargetBinding target{};
    target.colorTarget = slot.colorTarget.Get();
    target.colorRtv = slot.rtv;
    target.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    target.colorStateBefore = slot.colorState;
    target.colorStateAfter = readback ? D3D12_RESOURCE_STATE_COPY_SOURCE : D3D12_RESOURCE_STATE_RENDER_TARGET;
    target.transitionMode = directxsplat::ResourceTransitionMode::LibraryManaged;
    target.clearColor = true;
    const bool accumulationView = options.renderType == directxsplat::RenderType::Alpha ||
                                  options.renderType == directxsplat::RenderType::Depth;
    target.clearColorValue[0] = accumulationView ? 0.0f : options.background[0];
    target.clearColorValue[1] = accumulationView ? 0.0f : options.background[1];
    target.clearColorValue[2] = accumulationView ? 0.0f : options.background[2];
    target.clearColorValue[3] = accumulationView ? 0.0f : 1.0f;
    target.viewport.TopLeftX = 0.0f;
    target.viewport.TopLeftY = 0.0f;
    target.viewport.Width = static_cast<float>(width);
    target.viewport.Height = static_cast<float>(height);
    target.viewport.MinDepth = 0.0f;
    target.viewport.MaxDepth = 1.0f;
    target.scissor.left = 0;
    target.scissor.top = 0;
    target.scissor.right = static_cast<LONG>(width);
    target.scissor.bottom = static_cast<LONG>(height);
    return target;
  }

  directxsplat::Status ResetSlot(FrameSlot& slot) {
    HRESULT hr = slot.allocator->Reset();
    if (FAILED(hr)) {
      return HrStatus(hr, "failed resetting command allocator");
    }
    hr = slot.commandList->Reset(slot.allocator.Get(), nullptr);
    if (FAILED(hr)) {
      return HrStatus(hr, "failed resetting command list");
    }
    return directxsplat::Status::Ok();
  }

  directxsplat::Status SubmitSlot(FrameSlot& slot, directxsplat::UploadSyncPoint sync) {
    if (sync.IsValid()) {
      HRESULT hr = queue_->Wait(sync.fence, sync.value);
      if (FAILED(hr)) {
        return HrStatus(hr, "failed waiting for upload sync point");
      }
    }

    HRESULT hr = slot.commandList->Close();
    if (FAILED(hr)) {
      return HrStatus(hr, "failed closing command list");
    }

    ID3D12CommandList* lists[] = {slot.commandList.Get()};
    queue_->ExecuteCommandLists(1, lists);
    const uint64_t targetFence = fenceValue_ + 1;
    hr = queue_->Signal(fence_.Get(), targetFence);
    if (FAILED(hr)) {
      return HrStatus(hr, "failed signaling fence");
    }
    fenceValue_ = targetFence;
    slot.fenceValue = targetFence;
    return directxsplat::Status::Ok();
  }

  directxsplat::Status WaitForFenceValue(uint64_t fenceValue) {
    if (fenceValue == 0 || fence_->GetCompletedValue() >= fenceValue) {
      return directxsplat::Status::Ok();
    }
    directxsplat::Status deviceStatus = CheckDeviceRemoved(device_.Get());
    if (!deviceStatus.ok) {
      return deviceStatus;
    }
    HRESULT hr = fence_->SetEventOnCompletion(fenceValue, fenceEvent_);
    if (FAILED(hr)) {
      deviceStatus = CheckDeviceRemoved(device_.Get());
      return deviceStatus.ok ? HrStatus(hr, "failed waiting for fence") : deviceStatus;
    }
    while (fence_->GetCompletedValue() < fenceValue) {
      const DWORD wait = WaitForSingleObject(fenceEvent_, kFenceWaitPollMs);
      if (wait == WAIT_OBJECT_0) {
        break;
      }
      if (wait != WAIT_TIMEOUT) {
        return directxsplat::Status::Error("failed waiting for fence");
      }
      deviceStatus = CheckDeviceRemoved(device_.Get());
      if (!deviceStatus.ok) {
        return deviceStatus;
      }
    }
    return directxsplat::Status::Ok();
  }

  directxsplat::Status WaitForSlot(const FrameSlot& slot) {
    return WaitForFenceValue(slot.fenceValue);
  }

  directxsplat::Status WaitForAllSlots() {
    for (const FrameSlot& slot : slots_) {
      directxsplat::Status status = WaitForSlot(slot);
      if (!status.ok) {
        return status;
      }
    }
    return directxsplat::Status::Ok();
  }

  directxsplat::Status EnsureTimestampResources(uint32_t queryCount) {
    timestampHeap_.Reset();
    timestampReadback_.Reset();
    timestampQueryCount_ = queryCount;
    if (queryCount == 0) {
      return directxsplat::Status::Ok();
    }

    D3D12_QUERY_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    heapDesc.Count = queryCount;
    HRESULT hr = device_->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(timestampHeap_.GetAddressOf()));
    if (FAILED(hr)) {
      return HrStatus(hr, "failed creating timestamp query heap");
    }

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = static_cast<UINT64>(queryCount) * sizeof(uint64_t);
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = device_->CreateCommittedResource(&heapProps,
                                          D3D12_HEAP_FLAG_NONE,
                                          &desc,
                                          D3D12_RESOURCE_STATE_COPY_DEST,
                                          nullptr,
                                          IID_PPV_ARGS(timestampReadback_.GetAddressOf()));
    if (FAILED(hr)) {
      return HrStatus(hr, "failed creating timestamp readback");
    }
    return directxsplat::Status::Ok();
  }

  directxsplat::Status ResolveMeasuredTimestamps(uint32_t queryCount, std::vector<double>& outGpuMs) {
    if (queryCount == 0 || timestampHeap_ == nullptr || timestampReadback_ == nullptr || queryCount > timestampQueryCount_) {
      return directxsplat::Status::Error("invalid timestamp query range");
    }

    HRESULT hr = resolveAllocator_->Reset();
    if (FAILED(hr)) {
      return HrStatus(hr, "failed resetting resolve allocator");
    }
    hr = resolveList_->Reset(resolveAllocator_.Get(), nullptr);
    if (FAILED(hr)) {
      return HrStatus(hr, "failed resetting resolve command list");
    }
    resolveList_->ResolveQueryData(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, queryCount, timestampReadback_.Get(), 0);
    hr = resolveList_->Close();
    if (FAILED(hr)) {
      return HrStatus(hr, "failed closing resolve command list");
    }

    ID3D12CommandList* lists[] = {resolveList_.Get()};
    queue_->ExecuteCommandLists(1, lists);
    const uint64_t targetFence = fenceValue_ + 1;
    hr = queue_->Signal(fence_.Get(), targetFence);
    if (FAILED(hr)) {
      return HrStatus(hr, "failed signaling resolve fence");
    }
    fenceValue_ = targetFence;

    directxsplat::Status waitStatus = WaitForFenceValue(targetFence);
    if (!waitStatus.ok) {
      return waitStatus;
    }

    void* mapped = nullptr;
    hr = timestampReadback_->Map(0, nullptr, &mapped);
    if (FAILED(hr) || mapped == nullptr) {
      return directxsplat::Status::Error("failed mapping timestamp readback");
    }

    const uint64_t* values = reinterpret_cast<const uint64_t*>(mapped);
    outGpuMs.clear();
    outGpuMs.reserve(queryCount / 2u);
    for (uint32_t i = 0; i + 1u < queryCount; i += 2u) {
      const uint64_t start = values[i];
      const uint64_t end = values[i + 1u];
      const uint64_t delta = end >= start ? end - start : 0;
      outGpuMs.push_back(static_cast<double>(delta) * 1000.0 / static_cast<double>(timestampFrequency_));
    }
    timestampReadback_->Unmap(0, nullptr);
    return directxsplat::Status::Ok();
  }

  void QueueColorReadback(const FrameSlot& slot) {
    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = slot.colorTarget.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = slot.colorReadback.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = slot.footprint;

    slot.commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
  }

  directxsplat::StatusOr<directxsplat::ImageRgba8> ReadbackImage(const FrameSlot& slot) const {
    directxsplat::Status layoutStatus = ValidateReadbackLayout(width_, height_, slot.footprint.Footprint.RowPitch, slot.readbackBytes);
    if (!layoutStatus.ok) {
      return directxsplat::StatusOr<directxsplat::ImageRgba8>::Error(layoutStatus.message);
    }

    directxsplat::ImageRgba8 image{};
    image.width = width_;
    image.height = height_;
    const uint64_t rowBytes64 = static_cast<uint64_t>(width_) * kBytesPerPixel;
    const uint64_t pixelBytes64 = static_cast<uint64_t>(height_) * rowBytes64;
    if (pixelBytes64 > std::numeric_limits<size_t>::max()) {
      return directxsplat::StatusOr<directxsplat::ImageRgba8>::Error("image is too large");
    }
    image.pixels.resize(static_cast<size_t>(pixelBytes64));

    void* mapped = nullptr;
    const HRESULT hr = slot.colorReadback->Map(0, nullptr, &mapped);
    if (FAILED(hr) || mapped == nullptr) {
      return directxsplat::StatusOr<directxsplat::ImageRgba8>::Error("failed mapping readback");
    }

    const uint8_t* src = reinterpret_cast<const uint8_t*>(mapped);
    const size_t rowBytes = static_cast<size_t>(rowBytes64);
    for (uint32_t y = 0; y < height_; ++y) {
      const uint8_t* srcRow = src + static_cast<size_t>(y) * slot.footprint.Footprint.RowPitch;
      uint8_t* dstRow = image.pixels.data() + static_cast<size_t>(y) * rowBytes;
      std::memcpy(dstRow, srcRow, rowBytes);
    }
    slot.colorReadback->Unmap(0, nullptr);
    return directxsplat::StatusOr<directxsplat::ImageRgba8>::Ok(std::move(image));
  }

  bool initialized_ = false;
  bool targetsReady_ = false;
  ComPtr<IDXGIFactory6> factory_;
  ComPtr<IDXGIAdapter1> adapter_;
  ComPtr<ID3D12Device> device_;
  ComPtr<ID3D12CommandQueue> queue_;
  ComPtr<ID3D12Fence> fence_;
  HANDLE fenceEvent_ = nullptr;
  uint64_t fenceValue_ = 0;
  uint64_t frameIndex_ = 0;
  uint64_t timestampFrequency_ = 0;
  uint32_t timestampQueryCount_ = 0;
  std::string adapterName_;

  directxsplat::D3D12Context context_;
  directxsplat::Renderer renderer_;

  FrameSlot slots_[kFrameSlotCount];
  ComPtr<ID3D12DescriptorHeap> rtvHeap_;
  UINT rtvDescriptorSize_ = 0;
  uint32_t width_ = 0;
  uint32_t height_ = 0;

  ComPtr<ID3D12QueryHeap> timestampHeap_;
  ComPtr<ID3D12Resource> timestampReadback_;
  ComPtr<ID3D12CommandAllocator> resolveAllocator_;
  ComPtr<ID3D12GraphicsCommandList> resolveList_;
};

directxsplat::Status ValidateCameraSet(const directxsplat::CameraSet& cameras) {
  if (cameras.cameras.empty()) {
    return directxsplat::Status::Error("camera set is empty");
  }
  const uint32_t width = cameras.cameras.front().width;
  const uint32_t height = cameras.cameras.front().height;
  for (const directxsplat::CameraParams& camera : cameras.cameras) {
    if (camera.width != width || camera.height != height) {
      return directxsplat::Status::Error("benchmark cameras must share one resolution");
    }
  }
  return directxsplat::Status::Ok();
}

bool WantsHot(BenchMode mode) {
  return mode == BenchMode::HotRender || mode == BenchMode::Both;
}

bool WantsQuality(BenchMode mode) {
  return mode == BenchMode::Quality || mode == BenchMode::Both;
}

}

int main(int argc, char** argv) {
  auto parsed = ParseArgs(argc, argv);
  if (!parsed.ok()) {
    std::cerr << parsed.status.message << "\n";
    PrintUsage();
    return 2;
  }
  BenchOptions options = std::move(parsed.value);

  if (!options.outputDir.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(options.outputDir, ec);
    if (ec) {
      std::cerr << "failed creating output directory\n";
      return 2;
    }
  }
  if (!options.jsonPath.empty()) {
    std::error_code ec;
    const std::filesystem::path parent = options.jsonPath.parent_path();
    if (!parent.empty()) {
      std::filesystem::create_directories(parent, ec);
    }
  }

  auto splats = directxsplat::LoadFromFile(options.scenePath);
  if (!splats.ok()) {
    std::cerr << splats.status.message << "\n";
    return 1;
  }

  auto cameras = directxsplat::LoadCameraSet(options.cameraPath);
  if (!cameras.ok()) {
    std::cerr << cameras.status.message << "\n";
    return 1;
  }
  if (options.first != 0 && cameras.value.cameras.size() > options.first) {
    cameras.value.cameras.resize(options.first);
  }

  directxsplat::Status cameraStatus = ValidateCameraSet(cameras.value);
  if (!cameraStatus.ok) {
    std::cerr << cameraStatus.message << "\n";
    return 1;
  }

  BenchRuntime runtime;
  directxsplat::Status initStatus = runtime.Initialize();
  if (!initStatus.ok) {
    std::cerr << initStatus.message << "\n";
    return 1;
  }

  directxsplat::UploadedSceneHandle sceneHandle{};
  directxsplat::Status uploadStatus = runtime.Renderer().CreateUploadedScene(directxsplat::SceneFromSplats(splats.value), sceneHandle);
  if (!uploadStatus.ok) {
    std::cerr << uploadStatus.message << "\n";
    return 1;
  }

  BenchSummary summary{};
  summary.splats = splats.value.Size();
  summary.width = cameras.value.cameras.front().width;
  summary.height = cameras.value.cameras.front().height;
  summary.cameraCount = static_cast<uint32_t>(cameras.value.cameras.size());
  summary.adapterName = runtime.AdapterName();
  summary.uploadedSceneCreateCount = 1;

  if (WantsHot(options.mode)) {
    directxsplat::Status hotStatus = runtime.RunHotRender(sceneHandle, cameras.value.cameras, options, summary.hot);
    if (!hotStatus.ok) {
      std::cerr << hotStatus.message << "\n";
      (void)runtime.Renderer().DestroyUploadedScene(sceneHandle);
      return 1;
    }
  }

  if (WantsQuality(options.mode)) {
    directxsplat::Status qualityStatus = runtime.RunQuality(sceneHandle, cameras.value.cameras, options, summary.quality);
    if (!qualityStatus.ok) {
      std::cerr << qualityStatus.message << "\n";
      (void)runtime.Renderer().DestroyUploadedScene(sceneHandle);
      return 1;
    }
  }

  directxsplat::UploadedSceneInfo info{};
  if (runtime.Renderer().GetUploadedSceneInfo(sceneHandle, info).ok) {
    summary.residentGaussians = info.residentGaussians;
  }
  (void)runtime.Renderer().DestroyUploadedScene(sceneHandle);
  summary.uploadedSceneDestroyCount = 1;

  if (!options.jsonPath.empty()) {
    directxsplat::Status writeStatus = WriteSummaryJson(options.jsonPath, summary);
    if (!writeStatus.ok) {
      std::cerr << writeStatus.message << "\n";
      return 1;
    }
  }

  std::cout << "#points: " << summary.splats << "\n";
  std::cout << "resolution: " << summary.width << "x" << summary.height << "\n";
  std::cout << "#imgs: " << summary.cameraCount << "\n";
  std::cout << "timing scope: " << kTimingScope << "\n";
  if (summary.hot.available) {
    std::cout << "hot-render: " << kHotRenderLabel << "\n";
    std::cout << "warmup sweeps: " << summary.hot.warmupSweeps << "\n";
    std::cout << "measurement sweeps: " << summary.hot.measurementSweeps << "\n";
    std::cout << "GPU FPS: " << std::fixed << std::setprecision(2) << summary.hot.gpuFps << "\n";
    std::cout << "median GPU frame: " << std::fixed << std::setprecision(4) << summary.hot.medianGpuMs << " ms\n";
    std::cout << "p95 GPU frame: " << std::fixed << std::setprecision(4) << summary.hot.p95GpuMs << " ms\n";
  }
  if (summary.quality.available) {
    std::cout << "quality: " << kQualityLabel << "\n";
  }
  return 0;
}
