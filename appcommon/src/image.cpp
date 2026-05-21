#include "appcommon/image.h"

#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>

namespace appcommon {

namespace {

constexpr uint32_t kMaxPpmDimension = 65536u;
constexpr size_t kMaxPpmBytes = 512ull * 1024ull * 1024ull;

dxsplat::Status ComputePpmBytes(uint32_t width, uint32_t height, size_t& outBytes) {
  outBytes = 0;
  if (width == 0 || height == 0 || width > kMaxPpmDimension || height > kMaxPpmDimension) {
    return dxsplat::Status::Error("invalid ppm dimensions");
  }
  if (static_cast<uint64_t>(width) > std::numeric_limits<uint64_t>::max() / height) {
    return dxsplat::Status::Error("invalid ppm dimensions");
  }
  const uint64_t pixels = static_cast<uint64_t>(width) * height;
  if (pixels > std::numeric_limits<size_t>::max() / 4u || pixels > kMaxPpmBytes / 4u) {
    return dxsplat::Status::Error("ppm image is too large");
  }
  outBytes = static_cast<size_t>(pixels) * 4u;
  return dxsplat::Status::Ok();
}

}

dxsplat::Status SavePpm(const ImageRgba8& image, const std::string& path) {
  size_t imageBytes = 0;
  const dxsplat::Status sizeStatus = ComputePpmBytes(image.width, image.height, imageBytes);
  if (!sizeStatus.ok || image.Empty() || image.pixels.size() < imageBytes) {
    return dxsplat::Status::Error("invalid image");
  }

  std::ofstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return dxsplat::Status::Error("failed to open output");
  }

  file << "P6\n" << image.width << " " << image.height << "\n255\n";
  const size_t pixelCount = imageBytes / 4u;
  for (size_t i = 0; i < pixelCount; ++i) {
    file.put(static_cast<char>(image.pixels[i * 4 + 0]));
    file.put(static_cast<char>(image.pixels[i * 4 + 1]));
    file.put(static_cast<char>(image.pixels[i * 4 + 2]));
  }
  if (!file) {
    return dxsplat::Status::Error("failed to write output");
  }
  return dxsplat::Status::Ok();
}

dxsplat::StatusOr<ImageRgba8> LoadPpm(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return dxsplat::StatusOr<ImageRgba8>::Error("failed to open image");
  }

  std::string magic;
  file >> magic;
  if (magic != "P6") {
    return dxsplat::StatusOr<ImageRgba8>::Error("unsupported ppm magic");
  }

  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t maxValue = 0;
  file >> width >> height >> maxValue;
  if (!file) {
    return dxsplat::StatusOr<ImageRgba8>::Error("invalid ppm header");
  }
  const int separator = file.get();
  if (separator == std::char_traits<char>::eof()) {
    return dxsplat::StatusOr<ImageRgba8>::Error("invalid ppm header");
  }
  if (width == 0 || height == 0 || maxValue != 255) {
    return dxsplat::StatusOr<ImageRgba8>::Error("invalid ppm header");
  }
  size_t imageBytes = 0;
  const dxsplat::Status sizeStatus = ComputePpmBytes(width, height, imageBytes);
  if (!sizeStatus.ok) {
    return dxsplat::StatusOr<ImageRgba8>::Error(sizeStatus.message);
  }

  ImageRgba8 image{};
  image.width = width;
  image.height = height;
  try {
    image.pixels.resize(imageBytes, 255u);
  } catch (const std::bad_alloc&) {
    return dxsplat::StatusOr<ImageRgba8>::Error("ppm allocation failed");
  } catch (const std::length_error&) {
    return dxsplat::StatusOr<ImageRgba8>::Error("ppm image is too large");
  }

  const size_t pixelCount = imageBytes / 4u;
  for (size_t i = 0; i < pixelCount; ++i) {
    const int r = file.get();
    const int g = file.get();
    const int b = file.get();
    if (r == std::char_traits<char>::eof() || g == std::char_traits<char>::eof() || b == std::char_traits<char>::eof()) {
      return dxsplat::StatusOr<ImageRgba8>::Error("truncated ppm data");
    }
    image.pixels[i * 4 + 0] = static_cast<uint8_t>(r);
    image.pixels[i * 4 + 1] = static_cast<uint8_t>(g);
    image.pixels[i * 4 + 2] = static_cast<uint8_t>(b);
    image.pixels[i * 4 + 3] = 255u;
  }

  return dxsplat::StatusOr<ImageRgba8>::Ok(std::move(image));
}

}  // namespace appcommon
