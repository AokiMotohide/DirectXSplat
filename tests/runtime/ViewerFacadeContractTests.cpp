#include <doctest/doctest.h>

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

}  // namespace

TEST_CASE("Viewer cannot SetSplats before Initialize") {
  const std::filesystem::path dir = MakeTempDir("directxsplat_viewer_facade_contract");
  auto loaded = dxsplat::LoadFromPly(WriteTinyPly(dir));
  REQUIRE(loaded.ok());

  dxsplat::Viewer viewer;

  const dxsplat::Status status = viewer.SetSplats(loaded.value);

  CHECK_FALSE(status.ok);
  CHECK(status.message == "viewer is not initialized");

  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

TEST_CASE("Viewer rejects empty GaussianSplats") {
  dxsplat::Viewer viewer;
  const dxsplat::GaussianSplats splats;

  const dxsplat::Status status = viewer.SetSplats(splats);

  CHECK_FALSE(status.ok);
  CHECK(status.message == "splats are empty");
}

TEST_CASE("Viewer accepts empty CameraSet") {
  dxsplat::Viewer viewer;
  const dxsplat::CameraSet cameras;

  const dxsplat::Status status = viewer.SetCameras(cameras);

  CHECK(status.ok);
}
