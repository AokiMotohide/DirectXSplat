#include <doctest/doctest.h>

#include <cmath>

#include "app/CameraPathAnimator.h"

namespace {

dxsplat::CameraParams MakeCamera(float x, float y, float z) {
  dxsplat::CameraParams camera{};
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

dxsplat::CameraSet MakeCameraSet(size_t count) {
  dxsplat::CameraSet cameras{};
  for (size_t i = 0; i < count; ++i) {
    cameras.cameras.push_back(MakeCamera(static_cast<float>(i), static_cast<float>(i % 2u), 2.0f));
  }
  return cameras;
}

bool Finite(const dxsplat::CameraState& state) {
  return std::isfinite(state.position.x) && std::isfinite(state.position.y) && std::isfinite(state.position.z) &&
         std::isfinite(state.yaw) && std::isfinite(state.pitch) && std::isfinite(state.roll);
}

}

TEST_CASE("Animator with zero cameras returns false") {
  dxsplat::CameraPathAnimator animator;
  dxsplat::CameraState state{};

  CHECK_FALSE(animator.Evaluate(state));
}

TEST_CASE("Animator with one camera returns that camera") {
  dxsplat::CameraPathAnimator animator;
  animator.SetCameras(MakeCameraSet(1));

  dxsplat::CameraState state{};
  CHECK(animator.Evaluate(state));
  CHECK(state.position.x == doctest::Approx(0.0f));
  CHECK(state.position.y == doctest::Approx(0.0f));
  CHECK(state.position.z == doctest::Approx(2.0f));
}

TEST_CASE("Advance wraps by camera count") {
  dxsplat::CameraPathAnimator animator;
  animator.SetCameras(MakeCameraSet(4));
  animator.SetTime(3.75f);
  animator.Advance(1.0f, 1.0f);

  CHECK(animator.Time() >= 0.0f);
  CHECK(animator.Time() < 4.0f);
  CHECK(animator.Time() == doctest::Approx(0.75f));
}

TEST_CASE("Manual camera edit disables animation") {
  dxsplat::AnimationUiState state{};
  state.enabled = true;

  dxsplat::StopAnimationOnCameraEdit(state, true);

  CHECK_FALSE(state.enabled);
}

TEST_CASE("Interpolated pose is finite") {
  dxsplat::CameraPathAnimator animator;
  animator.SetCameras(MakeCameraSet(4));

  for (float time : {0.0f, 0.25f, 1.5f, 3.75f}) {
    animator.SetTime(time);
    dxsplat::CameraState state{};
    CHECK(animator.Evaluate(state));
    CHECK(Finite(state));
  }
}
