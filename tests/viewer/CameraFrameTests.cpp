#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstdint>

#include "api/CameraSetInternal.h"
#include "render/CameraFrameRenderer.h"

namespace {

dxsplat::CameraParams MakeCamera(float fx = 100.0f, float fy = 100.0f, float frameWidth = 100.0f,
                                 float frameHeight = 100.0f) {
  dxsplat::CameraParams camera{};
  camera.width = static_cast<uint32_t>(frameWidth);
  camera.height = static_cast<uint32_t>(frameHeight);
  camera.extrinsic = {
      1.0f, 0.0f, 0.0f, -1.0f,
      0.0f, 1.0f, 0.0f, -2.0f,
      0.0f, 0.0f, 1.0f, -3.0f,
      0.0f, 0.0f, 0.0f, 1.0f,
  };
  camera.intrinsic = {
      fx, 0.0f, frameWidth * 0.5f,
      0.0f, fy, frameHeight * 0.5f,
      0.0f, 0.0f, 1.0f,
  };
  return camera;
}

float DistanceFromEye(const dxsplat::CameraFrameVertex& eye, const dxsplat::CameraFrameVertex& vertex) {
  return dxsplat::Length(vertex.position - eye.position);
}

dxsplat::Vec3 TransformPoint(const dxsplat::Mat4& matrix, const dxsplat::Vec3& point) {
  const dxsplat::Vec4 local{point.x, point.y, point.z, 1.0f};
  const dxsplat::Vec4 transformed = dxsplat::Mul(matrix, local);
  return {transformed.x / transformed.w, transformed.y / transformed.w, transformed.z / transformed.w};
}

}  // namespace

TEST_CASE("BuildCameraFrameVertices builds eye and four corners") {
  const std::array<dxsplat::CameraFrameVertex, 5> vertices = dxsplat::BuildCameraFrameVertices(MakeCamera(), 1.0f);

  REQUIRE(vertices.size() == 5u);
  CHECK(vertices[0].position.x == doctest::Approx(1.0f));
  CHECK(vertices[0].position.y == doctest::Approx(2.0f));
  CHECK(vertices[0].position.z == doctest::Approx(3.0f));
}

TEST_CASE("BuildCameraFrameVertices changes corners with intrinsic") {
  const std::array<dxsplat::CameraFrameVertex, 5> narrow = dxsplat::BuildCameraFrameVertices(MakeCamera(200.0f), 1.0f);
  const std::array<dxsplat::CameraFrameVertex, 5> wide = dxsplat::BuildCameraFrameVertices(MakeCamera(50.0f), 1.0f);
  const float narrowOffset = std::abs(narrow[1].position.x - narrow[0].position.x);
  const float wideOffset = std::abs(wide[1].position.x - wide[0].position.x);

  CHECK(wideOffset > narrowOffset);
}

TEST_CASE("BuildCameraFrameVertices frame size scales corner offsets") {
  const std::array<dxsplat::CameraFrameVertex, 5> smallFrame = dxsplat::BuildCameraFrameVertices(MakeCamera(), 1.0f);
  const std::array<dxsplat::CameraFrameVertex, 5> largeFrame = dxsplat::BuildCameraFrameVertices(MakeCamera(), 2.0f);
  const float smallDistance = DistanceFromEye(smallFrame[0], smallFrame[1]);
  const float largeDistance = DistanceFromEye(largeFrame[0], largeFrame[1]);

  CHECK(largeDistance == doctest::Approx(smallDistance * 2.0f));
}

TEST_CASE("BuildCameraFrameModelMatrix matches generated vertices") {
  const dxsplat::CameraParams camera = MakeCamera(200.0f, 100.0f, 400.0f, 200.0f);
  const dxsplat::Mat4 model = dxsplat::BuildCameraFrameModelMatrix(camera, 2.0f);
  const std::array<dxsplat::CameraFrameVertex, 5> vertices = dxsplat::BuildCameraFrameVertices(camera, 2.0f);

  CHECK(TransformPoint(model, {-1.0f, -1.0f, 1.0f}).x == doctest::Approx(vertices[1].position.x));
  CHECK(TransformPoint(model, {-1.0f, -1.0f, 1.0f}).y == doctest::Approx(vertices[1].position.y));
  CHECK(TransformPoint(model, {-1.0f, -1.0f, 1.0f}).z == doctest::Approx(vertices[1].position.z));
}

TEST_CASE("ConvertInputCamerasToCameraSet preserves loaded matrix cameras") {
  dxsplat::Scene scene{};
  dxsplat::InputCamera input{};
  input.name = "matrix";
  input.width = 1920;
  input.height = 1080;
  input.hasMatrixParams = true;
  input.extrinsic.m = {
      1.0f, 2.0f, 3.0f, 4.0f,
      5.0f, 6.0f, 7.0f, 8.0f,
      9.0f, 10.0f, 11.0f, 12.0f,
      13.0f, 14.0f, 15.0f, 16.0f,
  };
  input.intrinsic.m = {
      100.0f, 0.0f, 960.0f,
      0.0f, 100.0f, 540.0f,
      0.0f, 0.0f, 1.0f,
  };
  scene.inputCameras.push_back(input);

  const auto converted = dxsplat::ConvertInputCamerasToCameraSet(scene);

  REQUIRE(converted.ok());
  REQUIRE(converted.value.cameras.size() == 1u);
  CHECK(converted.value.cameras[0].name == "matrix");
  CHECK(converted.value.cameras[0].width == 1920u);
  CHECK(converted.value.cameras[0].height == 1080u);
  CHECK(converted.value.cameras[0].extrinsic[11] == doctest::Approx(12.0f));
  CHECK(converted.value.cameras[0].intrinsic[2] == doctest::Approx(960.0f));
}

TEST_CASE("BuildCameraFrameIndices returns exact line list") {
  const std::array<uint32_t, 16> expected = {0, 1, 0, 2, 0, 3, 0, 4, 1, 2, 2, 4, 4, 3, 3, 1};
  CHECK(dxsplat::BuildCameraFrameIndices() == expected);
}
