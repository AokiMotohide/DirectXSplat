#pragma once

#include "platform/Image.h"

namespace directxsplat {

struct ImageComparison {
  double mae = 0.0;
  double mse = 0.0;
  double psnr = 0.0;
  double flipLike = 0.0;
};

ImageComparison CompareImages(const internal::ImageRgba8& a, const internal::ImageRgba8& b);
internal::ImageRgba8 BuildDiffImage(const internal::ImageRgba8& a, const internal::ImageRgba8& b);

}  // namespace directxsplat
