#include "metrics/ImageMetrics.h"

#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>

namespace dxsplat {
namespace {

ImageComparison InvalidComparison() {
  ImageComparison out{};
  out.mae = std::numeric_limits<double>::infinity();
  out.mse = std::numeric_limits<double>::infinity();
  out.psnr = 0.0;
  out.flipLike = std::numeric_limits<double>::infinity();
  return out;
}

bool ImageByteCount(const internal::ImageRgba8& image, size_t& byteCount) {
  if (image.Empty()) {
    return false;
  }
  const size_t width = image.width;
  const size_t height = image.height;
  if (height != 0 && width > std::numeric_limits<size_t>::max() / height) {
    return false;
  }
  const size_t pixels = width * height;
  if (pixels > std::numeric_limits<size_t>::max() / 4u) {
    return false;
  }
  byteCount = pixels * 4u;
  return image.pixels.size() >= byteCount;
}

}

ImageComparison CompareImages(const internal::ImageRgba8& a, const internal::ImageRgba8& b) {
  size_t byteCount = 0;
  size_t otherByteCount = 0;
  if (a.width != b.width || a.height != b.height || !ImageByteCount(a, byteCount) ||
      !ImageByteCount(b, otherByteCount) || byteCount != otherByteCount) {
    return InvalidComparison();
  }

  const size_t count = byteCount / 4u;
  double absSum = 0.0;
  double sqSum = 0.0;
  double perceptualSum = 0.0;
  ImageComparison out{};

  for (size_t i = 0; i < count; ++i) {
    const size_t o = i * 4;
    for (size_t c = 0; c < 3; ++c) {
      const double da = static_cast<double>(a.pixels[o + c]) / 255.0;
      const double db = static_cast<double>(b.pixels[o + c]) / 255.0;
      const double d = da - db;
      absSum += std::abs(d);
      sqSum += d * d;
      perceptualSum += std::sqrt(std::abs(d));
    }
  }

  const double denom = static_cast<double>(count) * 3.0;
  out.mae = absSum / denom;
  out.mse = sqSum / denom;
  out.flipLike = perceptualSum / denom;
  out.psnr = (out.mse <= 1e-12) ? 120.0 : 10.0 * std::log10(1.0 / out.mse);
  return out;
}

internal::ImageRgba8 BuildDiffImage(const internal::ImageRgba8& a, const internal::ImageRgba8& b) {
  internal::ImageRgba8 out{};
  size_t byteCount = 0;
  size_t otherByteCount = 0;
  if (a.width != b.width || a.height != b.height || !ImageByteCount(a, byteCount) ||
      !ImageByteCount(b, otherByteCount) || byteCount != otherByteCount) {
    return out;
  }

  out.width = a.width;
  out.height = a.height;
  try {
    out.pixels.resize(byteCount, 255);
  } catch (const std::bad_alloc&) {
    return {};
  } catch (const std::length_error&) {
    return {};
  }
  for (size_t i = 0; i < byteCount / 4u; ++i) {
    const size_t o = i * 4;
    for (size_t c = 0; c < 3; ++c) {
      const int da = static_cast<int>(a.pixels[o + c]);
      const int db = static_cast<int>(b.pixels[o + c]);
      const uint8_t diff = static_cast<uint8_t>(std::abs(da - db));
      out.pixels[o + c] = diff;
    }
    out.pixels[o + 3] = 255;
  }
  return out;
}

}  // namespace dxsplat
