#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include <dxsplat/directxsplat.h>

namespace {

std::filesystem::path MakeTempDir(const char* name) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / name;
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  return dir;
}

std::filesystem::path WriteTinyPly(const std::filesystem::path& dir) {
  const std::filesystem::path path = dir / "scene.ply";
  std::ofstream file(path, std::ios::binary);
  file << "ply\n"
       << "format ascii 1.0\n"
       << "element vertex 1\n"
       << "property float x\n"
       << "property float y\n"
       << "property float z\n"
       << "property float scale_0\n"
       << "property float scale_1\n"
       << "property float scale_2\n"
       << "property float rot_0\n"
       << "property float rot_1\n"
       << "property float rot_2\n"
       << "property float rot_3\n"
       << "property float opacity\n"
       << "property float f_dc_0\n"
       << "property float f_dc_1\n"
       << "property float f_dc_2\n"
       << "end_header\n"
       << "0 0 2 0.1 0.1 0.1 1 0 0 0 1 0 0 0\n";
  return path;
}

dxsplat::GaussianSplats LoadTinySplats(const char* name) {
  const std::filesystem::path dir = MakeTempDir(name);
  auto loaded = dxsplat::LoadFromPly(WriteTinyPly(dir));
  REQUIRE(loaded.ok());
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  return std::move(loaded.value);
}

dxsplat::CameraParams MakeCamera(uint32_t width = 64, uint32_t height = 64) {
  dxsplat::CameraParams camera{};
  camera.width = width;
  camera.height = height;
  camera.extrinsic = {
      1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, -1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f,
  };
  camera.intrinsic = {
      64.0f, 0.0f, static_cast<float>(width) * 0.5f,
      0.0f, 64.0f, static_cast<float>(height) * 0.5f,
      0.0f, 0.0f, 1.0f,
  };
  return camera;
}

}  // namespace

TEST_CASE("Draw rejects empty splats") {
  const dxsplat::GaussianSplats splats;
  const auto image = dxsplat::Draw(splats, MakeCamera());

  CHECK_FALSE(image.ok());
  CHECK(image.status.message == "splats are empty");
}

TEST_CASE("Draw rejects zero output size") {
  const dxsplat::GaussianSplats splats = LoadTinySplats("directxsplat_draw_zero_output");
  dxsplat::DrawOptions options{};
  options.width = 0;

  auto image = dxsplat::Draw(splats, MakeCamera(), options);

  CHECK_FALSE(image.ok());
  CHECK(image.status.message == "draw width must be greater than zero");

  options.width = 64;
  options.height = 0;
  image = dxsplat::Draw(splats, MakeCamera(), options);

  CHECK_FALSE(image.ok());
  CHECK(image.status.message == "draw height must be greater than zero");
}

TEST_CASE("Draw rejects zero camera size") {
  const dxsplat::GaussianSplats splats = LoadTinySplats("directxsplat_draw_zero_camera");
  dxsplat::CameraParams camera = MakeCamera(0, 64);

  auto image = dxsplat::Draw(splats, camera);

  CHECK_FALSE(image.ok());
  CHECK(image.status.message == "camera width must be greater than zero");

  camera = MakeCamera(64, 0);
  image = dxsplat::Draw(splats, camera);

  CHECK_FALSE(image.ok());
  CHECK(image.status.message == "camera height must be greater than zero");
}

TEST_CASE("Draw rejects invalid near and far planes") {
  const dxsplat::GaussianSplats splats = LoadTinySplats("directxsplat_draw_bad_planes");
  dxsplat::DrawOptions options{};
  options.nearPlane = 2.0f;
  options.farPlane = 1.0f;

  const auto image = dxsplat::Draw(splats, MakeCamera(), options);

  CHECK_FALSE(image.ok());
  CHECK(image.status.message == "far plane must be greater than near plane");
}

#if DXSPLAT_ENABLE_GPU_TESTS
TEST_CASE("Draw renders a tiny scene offscreen") {
  const dxsplat::GaussianSplats splats = LoadTinySplats("directxsplat_draw_gpu_tiny");
  dxsplat::DrawOptions options{};
  options.width = 64;
  options.height = 64;

  const auto image = dxsplat::Draw(splats, MakeCamera(), options);

  REQUIRE_MESSAGE(image.ok(), image.status.message);
  CHECK(image.value.width == 64);
  CHECK(image.value.height == 64);
  REQUIRE(image.value.pixels.size() == 64u * 64u * 4u);
  bool anyAlpha = false;
  for (size_t index = 3; index < image.value.pixels.size(); index += 4) {
    anyAlpha = anyAlpha || image.value.pixels[index] != 0;
  }
  CHECK(anyAlpha);
}

#endif
