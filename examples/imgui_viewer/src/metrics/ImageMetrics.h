#pragma once

#include "appcommon/image.h"

namespace dxsplat {

struct ImageComparison {
  double mae = 0.0;
  double mse = 0.0;
  double psnr = 0.0;
  double flipLike = 0.0;
};

ImageComparison CompareImages(const appcommon::ImageRgba8& a, const appcommon::ImageRgba8& b);
appcommon::ImageRgba8 BuildDiffImage(const appcommon::ImageRgba8& a, const appcommon::ImageRgba8& b);

}  // namespace dxsplat
