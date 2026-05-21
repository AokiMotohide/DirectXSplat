#include "io/image/wic_image.h"

#include <iomanip>
#include <limits>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "filesystem.hpp"

namespace dxsplat::io {

namespace {

using Microsoft::WRL::ComPtr;
namespace fs = ghc::filesystem;

constexpr uint32_t kMaxImageDimension = 65536;
constexpr size_t kMaxDecodedImageBytes = 512ull * 1024ull * 1024ull;

struct ScopedComInit {
  ScopedComInit() : hr(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
  ~ScopedComInit() {
    if (hr == S_OK || hr == S_FALSE) {
      CoUninitialize();
    }
  }

  HRESULT hr = E_FAIL;
};

std::string FormatHresult(HRESULT hr) {
  std::ostringstream stream;
  stream << "0x" << std::hex << std::setw(8) << std::setfill('0') << static_cast<uint32_t>(hr);
  return stream.str();
}

Status ComputeDecodedImageBytes(uint64_t width, uint64_t height, size_t& outBytes) {
  outBytes = 0;
  if (width == 0 || height == 0 || width > kMaxImageDimension || height > kMaxImageDimension) {
    return Status::Error("image dimensions are too large");
  }
  if (width > std::numeric_limits<uint64_t>::max() / height) {
    return Status::Error("image dimensions are too large");
  }
  const uint64_t pixels = width * height;
  if (pixels > std::numeric_limits<size_t>::max() / 4u || pixels > kMaxDecodedImageBytes / 4u) {
    return Status::Error("image dimensions are too large");
  }
  outBytes = static_cast<size_t>(pixels * 4u);
  return Status::Ok();
}

StatusOr<DecodedImage> DecodeWithStbFile(const std::string& path) {
  int width = 0;
  int height = 0;
  int channels = 0;
  if (!stbi_info(path.c_str(), &width, &height, &channels)) {
    const char* reason = stbi_failure_reason();
    return StatusOr<DecodedImage>::Error(reason != nullptr ? reason : "failed to decode image");
  }
  size_t imageBytes = 0;
  const Status sizeStatus = ComputeDecodedImageBytes(static_cast<uint64_t>(width), static_cast<uint64_t>(height), imageBytes);
  if (!sizeStatus.ok) {
    return StatusOr<DecodedImage>::Error(sizeStatus.message);
  }
  stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
  if (data == nullptr) {
    const char* reason = stbi_failure_reason();
    return StatusOr<DecodedImage>::Error(reason != nullptr ? reason : "failed to decode image");
  }
  size_t decodedBytes = 0;
  const Status decodedSizeStatus = ComputeDecodedImageBytes(static_cast<uint64_t>(width), static_cast<uint64_t>(height), decodedBytes);
  if (!decodedSizeStatus.ok) {
    stbi_image_free(data);
    return StatusOr<DecodedImage>::Error(decodedSizeStatus.message);
  }
  imageBytes = decodedBytes;

  DecodedImage out{};
  out.width = static_cast<uint32_t>(width);
  out.height = static_cast<uint32_t>(height);
  try {
    out.rgba.assign(data, data + imageBytes);
  } catch (const std::bad_alloc&) {
    stbi_image_free(data);
    return StatusOr<DecodedImage>::Error("image allocation failed");
  } catch (const std::length_error&) {
    stbi_image_free(data);
    return StatusOr<DecodedImage>::Error("image dimensions are too large");
  }
  stbi_image_free(data);

  return StatusOr<DecodedImage>::Ok(std::move(out));
}

StatusOr<DecodedImage> DecodeWithStbMemory(const std::vector<uint8_t>& bytes) {
  if (bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return StatusOr<DecodedImage>::Error("image data is too large");
  }
  const int byteCount = static_cast<int>(bytes.size());
  int width = 0;
  int height = 0;
  int channels = 0;
  if (!stbi_info_from_memory(bytes.data(), byteCount, &width, &height, &channels)) {
    const char* reason = stbi_failure_reason();
    return StatusOr<DecodedImage>::Error(reason != nullptr ? reason : "failed to decode image from memory");
  }
  size_t imageBytes = 0;
  const Status sizeStatus = ComputeDecodedImageBytes(static_cast<uint64_t>(width), static_cast<uint64_t>(height), imageBytes);
  if (!sizeStatus.ok) {
    return StatusOr<DecodedImage>::Error(sizeStatus.message);
  }
  stbi_uc* data = stbi_load_from_memory(bytes.data(), byteCount, &width, &height, &channels, STBI_rgb_alpha);
  if (data == nullptr) {
    const char* reason = stbi_failure_reason();
    return StatusOr<DecodedImage>::Error(reason != nullptr ? reason : "failed to decode image from memory");
  }
  size_t decodedBytes = 0;
  const Status decodedSizeStatus = ComputeDecodedImageBytes(static_cast<uint64_t>(width), static_cast<uint64_t>(height), decodedBytes);
  if (!decodedSizeStatus.ok) {
    stbi_image_free(data);
    return StatusOr<DecodedImage>::Error(decodedSizeStatus.message);
  }
  imageBytes = decodedBytes;

  DecodedImage out{};
  out.width = static_cast<uint32_t>(width);
  out.height = static_cast<uint32_t>(height);
  try {
    out.rgba.assign(data, data + imageBytes);
  } catch (const std::bad_alloc&) {
    stbi_image_free(data);
    return StatusOr<DecodedImage>::Error("image allocation failed");
  } catch (const std::length_error&) {
    stbi_image_free(data);
    return StatusOr<DecodedImage>::Error("image dimensions are too large");
  }
  stbi_image_free(data);

  return StatusOr<DecodedImage>::Ok(std::move(out));
}

StatusOr<DecodedImage> DecodeFrameWithWic(const ComPtr<IWICImagingFactory>& factory, const ComPtr<IWICBitmapFrameDecode>& frame) {
  ComPtr<IWICFormatConverter> converter;
  HRESULT hr = factory->CreateFormatConverter(&converter);
  if (FAILED(hr)) {
    return StatusOr<DecodedImage>::Error("failed to create WIC format converter: " + FormatHresult(hr));
  }

  hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
                             WICBitmapPaletteTypeCustom);
  if (FAILED(hr)) {
    return StatusOr<DecodedImage>::Error("failed to initialize WIC format converter: " + FormatHresult(hr));
  }

  UINT width = 0;
  UINT height = 0;
  hr = converter->GetSize(&width, &height);
  if (FAILED(hr) || width == 0 || height == 0) {
    return StatusOr<DecodedImage>::Error("failed to query WIC image size");
  }

  DecodedImage out{};
  out.width = width;
  out.height = height;
  size_t imageBytes = 0;
  Status sizeStatus = ComputeDecodedImageBytes(width, height, imageBytes);
  if (!sizeStatus.ok) {
    return StatusOr<DecodedImage>::Error(sizeStatus.message);
  }
  try {
    out.rgba.resize(imageBytes);
  } catch (const std::bad_alloc&) {
    return StatusOr<DecodedImage>::Error("image allocation failed");
  } catch (const std::length_error&) {
    return StatusOr<DecodedImage>::Error("image dimensions are too large");
  }

  if (width > std::numeric_limits<UINT>::max() / 4u || out.rgba.size() > std::numeric_limits<UINT>::max()) {
    return StatusOr<DecodedImage>::Error("image dimensions are too large");
  }
  hr = converter->CopyPixels(nullptr, width * 4u, static_cast<UINT>(out.rgba.size()), out.rgba.data());
  if (FAILED(hr)) {
    return StatusOr<DecodedImage>::Error("failed to copy WIC pixels: " + FormatHresult(hr));
  }

  return StatusOr<DecodedImage>::Ok(std::move(out));
}

StatusOr<ComPtr<IWICImagingFactory>> CreateWicFactory() {
  ComPtr<IWICImagingFactory> factory;
  const HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
  if (FAILED(hr) || factory == nullptr) {
    return StatusOr<ComPtr<IWICImagingFactory>>::Error("failed to create WIC factory: " + FormatHresult(hr));
  }

  return StatusOr<ComPtr<IWICImagingFactory>>::Ok(std::move(factory));
}

StatusOr<DecodedImage> DecodeWithWicFile(const std::string& path) {
  ScopedComInit comInit;
  if (FAILED(comInit.hr) && comInit.hr != RPC_E_CHANGED_MODE) {
    return StatusOr<DecodedImage>::Error("failed to initialize COM: " + FormatHresult(comInit.hr));
  }

  const auto factoryResult = CreateWicFactory();
  if (!factoryResult.ok()) {
    return StatusOr<DecodedImage>::Error(factoryResult.status.message);
  }

  const fs::path fsPath(path);
  ComPtr<IWICBitmapDecoder> decoder;
  const HRESULT hr = factoryResult.value->CreateDecoderFromFilename(fsPath.wstring().c_str(), nullptr, GENERIC_READ,
                                                                   WICDecodeMetadataCacheOnLoad, &decoder);
  if (FAILED(hr) || decoder == nullptr) {
    return StatusOr<DecodedImage>::Error("failed to create WIC decoder from file: " + FormatHresult(hr));
  }

  ComPtr<IWICBitmapFrameDecode> frame;
  if (FAILED(decoder->GetFrame(0, &frame)) || frame == nullptr) {
    return StatusOr<DecodedImage>::Error("failed to get WIC frame");
  }

  return DecodeFrameWithWic(factoryResult.value, frame);
}

StatusOr<DecodedImage> DecodeWithWicMemory(const std::vector<uint8_t>& bytes) {
  if (bytes.empty()) {
    return StatusOr<DecodedImage>::Error("image buffer is empty");
  }

  ScopedComInit comInit;
  if (FAILED(comInit.hr) && comInit.hr != RPC_E_CHANGED_MODE) {
    return StatusOr<DecodedImage>::Error("failed to initialize COM: " + FormatHresult(comInit.hr));
  }

  const auto factoryResult = CreateWicFactory();
  if (!factoryResult.ok()) {
    return StatusOr<DecodedImage>::Error(factoryResult.status.message);
  }

  ComPtr<IWICStream> stream;
  HRESULT hr = factoryResult.value->CreateStream(&stream);
  if (FAILED(hr) || stream == nullptr) {
    return StatusOr<DecodedImage>::Error("failed to create WIC stream: " + FormatHresult(hr));
  }

  hr = stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size()));
  if (FAILED(hr)) {
    return StatusOr<DecodedImage>::Error("failed to initialize WIC stream: " + FormatHresult(hr));
  }

  ComPtr<IWICBitmapDecoder> decoder;
  hr = factoryResult.value->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
  if (FAILED(hr) || decoder == nullptr) {
    return StatusOr<DecodedImage>::Error("failed to create WIC decoder from memory: " + FormatHresult(hr));
  }

  ComPtr<IWICBitmapFrameDecode> frame;
  if (FAILED(decoder->GetFrame(0, &frame)) || frame == nullptr) {
    return StatusOr<DecodedImage>::Error("failed to get WIC frame");
  }

  return DecodeFrameWithWic(factoryResult.value, frame);
}

}  

StatusOr<DecodedImage> DecodeImageFromFileWic(const std::string& path) {
  const auto wicResult = DecodeWithWicFile(path);
  if (wicResult.ok()) {
    return wicResult;
  }
  return DecodeWithStbFile(path);
}

StatusOr<DecodedImage> DecodeImageFromMemoryWic(const std::vector<uint8_t>& bytes) {
  const auto wicResult = DecodeWithWicMemory(bytes);
  if (wicResult.ok()) {
    return wicResult;
  }
  return DecodeWithStbMemory(bytes);
}

}
