#include <doctest/doctest.h>

#include <dxsplat/directxsplat.h>

TEST_CASE("GaussianSplats default object is empty") {
  directxsplat::GaussianSplats splats;

  CHECK(splats.Size() == 0);
  CHECK(splats.Empty());
}

TEST_CASE("DrawOptions defaults match viewer defaults") {
  directxsplat::DrawOptions options{};

  CHECK(options.width == 1600);
  CHECK(options.height == 900);
  CHECK(options.nearPlane == doctest::Approx(0.1f));
  CHECK(options.farPlane == doctest::Approx(5000.0f));
  CHECK(options.background[0] == doctest::Approx(0.0f));
  CHECK(options.background[1] == doctest::Approx(0.0f));
  CHECK(options.background[2] == doctest::Approx(0.0f));
  CHECK(options.antialiasing);
  CHECK(options.antialiasingStrength == doctest::Approx(1.0f));
  CHECK_FALSE(options.gammaCorrection);
  CHECK(options.renderType == directxsplat::RenderType::Color);
  CHECK(options.shadingDegree == directxsplat::ShadingDegree::Degree3);
}
