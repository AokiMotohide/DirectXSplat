#include <doctest/doctest.h>

#include <cmath>

#include "app/CameraPathAnimator.h"

namespace {

directxsplat::CameraParams MakeCamera(float x, float y, float z) {
  directxsplat::CameraParams camera{};
  camera.width = 100;
  camera.height = 100;
  camera.intrinsic = {
      100.0f, 0.0f, 50.0f,
      0.0f, 100.0f, 50.0f,
      0.0f, 0.0f, 1.0f,
  };
  camera.extrinsic = {
      1.0f, 0.0f, 0.0f, -x,
      0.0f, -1.0f, 0.0f, y,
      0.0f, 0.0f, 1.0f, -z,
      0.0f, 0.0f, 0.0f, 1.0f,
  };
  return camera;
}

directxsplat::CameraSet MakeCameraSet(size_t count) {
  directxsplat::CameraSet cameras{};
  for (size_t i = 0; i < count; ++i) {
    cameras.cameras.push_back(MakeCamera(static_cast<float>(i), static_cast<float>(i % 2u), 2.0f));
  }
  return cameras;
}

bool Finite(const directxsplat::CameraState& state) {
  return std::isfinite(state.position.x) && std::isfinite(state.position.y) && std::isfinite(state.position.z) &&
         std::isfinite(state.yaw) && std::isfinite(state.pitch) && std::isfinite(state.roll);
}

}

TEST_CASE("Animator with zero cameras returns false") {
  directxsplat::CameraPathAnimator animator;
  directxsplat::CameraState state{};

  CHECK_FALSE(animator.Evaluate(state));
}

TEST_CASE("Animator with one camera returns that camera") {
  directxsplat::CameraPathAnimator animator;
  animator.SetCameras(MakeCameraSet(1));

  directxsplat::CameraState state{};
  CHECK(animator.Evaluate(state));
  CHECK(state.position.x == doctest::Approx(0.0f));
  CHECK(state.position.y == doctest::Approx(0.0f));
  CHECK(state.position.z == doctest::Approx(2.0f));
}

TEST_CASE("Advance wraps by camera count") {
  directxsplat::CameraPathAnimator animator;
  animator.SetCameras(MakeCameraSet(4));
  animator.SetTime(3.75f);
  animator.Advance(1.0f, 1.0f);

  CHECK(animator.Time() >= 0.0f);
  CHECK(animator.Time() < 4.0f);
  CHECK(animator.Time() == doctest::Approx(0.75f));
}

TEST_CASE("Manual camera edit disables animation") {
  directxsplat::AnimationUiState state{};
  state.enabled = true;

  directxsplat::StopAnimationOnCameraEdit(state, true);

  CHECK_FALSE(state.enabled);
}

TEST_CASE("Interpolated pose is finite") {
  directxsplat::CameraPathAnimator animator;
  animator.SetCameras(MakeCameraSet(4));

  for (float time : {0.0f, 0.25f, 1.5f, 3.75f}) {
    animator.SetTime(time);
    directxsplat::CameraState state{};
    CHECK(animator.Evaluate(state));
    CHECK(Finite(state));
  }
}

TEST_CASE("Camera snap uses default viewer fov") {
  directxsplat::CameraController controller;
  directxsplat::CameraState state = controller.State();
  state.fovYRadians = 0.75f;
  controller.SetState(state);

  controller.SnapToCameraParams(MakeCamera(1.0f, 2.0f, 3.0f));

  CHECK(controller.State().position.x == doctest::Approx(1.0f));
  CHECK(controller.State().position.y == doctest::Approx(2.0f));
  CHECK(controller.State().position.z == doctest::Approx(3.0f));
  CHECK(controller.State().fovYRadians == doctest::Approx(directxsplat::kDefaultCameraFovYRadians));
}
