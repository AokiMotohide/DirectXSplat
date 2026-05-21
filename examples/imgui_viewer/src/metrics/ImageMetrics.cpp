#include "metrics/ImageMetrics.h"

#include <cmath>

namespace dxsplat {

ImageComparison CompareImages(const appcommon::ImageRgba8& a, const appcommon::ImageRgba8& b) {
  ImageComparison out{};
  if (a.width != b.width || a.height != b.height || a.Empty() || b.Empty()) {
    out.mae = std::numeric_limits<double>::infinity();
    out.mse = std::numeric_limits<double>::infinity();
    out.psnr = 0.0;
    out.flipLike = std::numeric_limits<double>::infinity();
    return out;
  }

  const size_t count = static_cast<size_t>(a.width) * a.height;
  double absSum = 0.0;
  double sqSum = 0.0;
  double perceptualSum = 0.0;

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

appcommon::ImageRgba8 BuildDiffImage(const appcommon::ImageRgba8& a, const appcommon::ImageRgba8& b) {
  appcommon::ImageRgba8 out{};
  if (a.width != b.width || a.height != b.height || a.Empty() || b.Empty()) {
    return out;
  }

  out.width = a.width;
  out.height = a.height;
  out.pixels.resize(static_cast<size_t>(out.width) * out.height * 4, 255);
  for (size_t i = 0; i < static_cast<size_t>(out.width) * out.height; ++i) {
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
