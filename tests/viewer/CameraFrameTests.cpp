#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstdint>

#include "api/CameraSetInternal.h"
#include "render/CameraFrameRenderer.h"

namespace {

directxsplat::CameraParams MakeCamera(float fx = 100.0f, float fy = 100.0f, float frameWidth = 100.0f,
                                 float frameHeight = 100.0f) {
  directxsplat::CameraParams camera{};
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

float DistanceFromEye(const directxsplat::CameraFrameVertex& eye, const directxsplat::CameraFrameVertex& vertex) {
  return directxsplat::Length(vertex.position - eye.position);
}

directxsplat::Vec3 TransformPoint(const directxsplat::Mat4& matrix, const directxsplat::Vec3& point) {
  const directxsplat::Vec4 local{point.x, point.y, point.z, 1.0f};
  const directxsplat::Vec4 transformed = directxsplat::Mul(matrix, local);
  return {transformed.x / transformed.w, transformed.y / transformed.w, transformed.z / transformed.w};
}

}  // namespace

TEST_CASE("BuildCameraFrameVertices builds eye and four corners") {
  const std::array<directxsplat::CameraFrameVertex, 5> vertices = directxsplat::BuildCameraFrameVertices(MakeCamera(), 1.0f);

  REQUIRE(vertices.size() == 5u);
  CHECK(vertices[0].position.x == doctest::Approx(1.0f));
  CHECK(vertices[0].position.y == doctest::Approx(2.0f));
  CHECK(vertices[0].position.z == doctest::Approx(3.0f));
}

TEST_CASE("BuildCameraFrameVertices changes corners with intrinsic") {
  const std::array<directxsplat::CameraFrameVertex, 5> narrow = directxsplat::BuildCameraFrameVertices(MakeCamera(200.0f), 1.0f);
  const std::array<directxsplat::CameraFrameVertex, 5> wide = directxsplat::BuildCameraFrameVertices(MakeCamera(50.0f), 1.0f);
  const float narrowOffset = std::abs(narrow[1].position.x - narrow[0].position.x);
  const float wideOffset = std::abs(wide[1].position.x - wide[0].position.x);

  CHECK(wideOffset > narrowOffset);
}

TEST_CASE("BuildCameraFrameVertices frame size scales corner offsets") {
  const std::array<directxsplat::CameraFrameVertex, 5> smallFrame = directxsplat::BuildCameraFrameVertices(MakeCamera(), 1.0f);
  const std::array<directxsplat::CameraFrameVertex, 5> largeFrame = directxsplat::BuildCameraFrameVertices(MakeCamera(), 2.0f);
  const float smallDistance = DistanceFromEye(smallFrame[0], smallFrame[1]);
  const float largeDistance = DistanceFromEye(largeFrame[0], largeFrame[1]);

  CHECK(largeDistance == doctest::Approx(smallDistance * 2.0f));
}

TEST_CASE("BuildCameraFrameModelMatrix matches generated vertices") {
  const directxsplat::CameraParams camera = MakeCamera(200.0f, 100.0f, 400.0f, 200.0f);
  const directxsplat::Mat4 model = directxsplat::BuildCameraFrameModelMatrix(camera, 2.0f);
  const std::array<directxsplat::CameraFrameVertex, 5> vertices = directxsplat::BuildCameraFrameVertices(camera, 2.0f);

  CHECK(TransformPoint(model, {-1.0f, -1.0f, 1.0f}).x == doctest::Approx(vertices[1].position.x));
  CHECK(TransformPoint(model, {-1.0f, -1.0f, 1.0f}).y == doctest::Approx(vertices[1].position.y));
  CHECK(TransformPoint(model, {-1.0f, -1.0f, 1.0f}).z == doctest::Approx(vertices[1].position.z));
}

TEST_CASE("CameraRenderStateFromCameraParams preserves camera y down") {
  const directxsplat::CameraParams camera = MakeCamera();
  const directxsplat::CameraRenderState state = directxsplat::CameraRenderStateFromCameraParams(camera, 0.1f, 100.0f);

  CHECK(state.view.m[4] == doctest::Approx(camera.extrinsic[4]));
  CHECK(state.view.m[5] == doctest::Approx(camera.extrinsic[5]));
  CHECK(state.view.m[6] == doctest::Approx(camera.extrinsic[6]));
  CHECK(state.view.m[7] == doctest::Approx(camera.extrinsic[7]));
  CHECK(state.proj.m[5] < 0.0f);
}

TEST_CASE("ConvertInputCamerasToCameraSet preserves loaded matrix cameras") {
  directxsplat::Scene scene{};
  directxsplat::InputCamera input{};
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

  const auto converted = directxsplat::ConvertInputCamerasToCameraSet(scene);

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
  CHECK(directxsplat::BuildCameraFrameIndices() == expected);
}
