#include <doctest/doctest.h>

#include <array>
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

void WriteFile(const std::filesystem::path& path, const std::string& data) {
  std::ofstream file(path, std::ios::binary);
  file << data;
}

}  // namespace

TEST_CASE("LoadFromPly rejects empty path") {
  const auto loaded = dxsplat::LoadFromPly({});

  CHECK_FALSE(loaded.ok());
  CHECK(loaded.status.message == "scene path is empty");
}

TEST_CASE("LoadFromPly rejects .txt path") {
  const auto loaded = dxsplat::LoadFromPly("scene.txt");

  CHECK_FALSE(loaded.ok());
  CHECK(loaded.status.message.find("expected .ply") != std::string::npos);
}

TEST_CASE("LoadCameraSet reads camera json") {
  const std::filesystem::path dir = MakeTempDir("directxsplat_facade_camera_json");
  const std::filesystem::path path = dir / "cameras.json";
  WriteFile(path,
            "["
            "{"
            "\"name\":\"camera 0\","
            "\"extrinsic\":[[1,2,3,4],[5,6,7,8],[9,10,11,12],[13,14,15,16]],"
            "\"intrinsic\":[[101,0,320],[0,202,240],[0,0,1]],"
            "\"width\":640,"
            "\"height\":480"
            "},"
            "{"
            "\"name\":\"camera 1\","
            "\"extrinsic\":[[17,18,19,20],[21,22,23,24],[25,26,27,28],[29,30,31,32]],"
            "\"intrinsic\":[[303,0,400],[0,404,300],[0,0,1]],"
            "\"width\":800,"
            "\"height\":600"
            "}"
            "]");

  const auto loaded = dxsplat::LoadCameraSet(path);

  REQUIRE(loaded.ok());
  REQUIRE(loaded.value.cameras.size() == 2u);
  const std::array<float, 16> expectedExtrinsic0{
      1.0f, 2.0f, 3.0f, 4.0f,
      5.0f, 6.0f, 7.0f, 8.0f,
      9.0f, 10.0f, 11.0f, 12.0f,
      13.0f, 14.0f, 15.0f, 16.0f,
  };
  const std::array<float, 9> expectedIntrinsic0{
      101.0f, 0.0f, 320.0f,
      0.0f, 202.0f, 240.0f,
      0.0f, 0.0f, 1.0f,
  };
  const std::array<float, 16> expectedExtrinsic1{
      17.0f, 18.0f, 19.0f, 20.0f,
      21.0f, 22.0f, 23.0f, 24.0f,
      25.0f, 26.0f, 27.0f, 28.0f,
      29.0f, 30.0f, 31.0f, 32.0f,
  };
  const std::array<float, 9> expectedIntrinsic1{
      303.0f, 0.0f, 400.0f,
      0.0f, 404.0f, 300.0f,
      0.0f, 0.0f, 1.0f,
  };

  CHECK(loaded.value.cameras[0].name == "camera 0");
  CHECK(loaded.value.cameras[0].width == 640);
  CHECK(loaded.value.cameras[0].height == 480);
  CHECK(loaded.value.cameras[1].width == 800);
  CHECK(loaded.value.cameras[1].height == 600);
  for (size_t index = 0; index < expectedExtrinsic0.size(); ++index) {
    CHECK(loaded.value.cameras[0].extrinsic[index] == doctest::Approx(expectedExtrinsic0[index]));
    CHECK(loaded.value.cameras[1].extrinsic[index] == doctest::Approx(expectedExtrinsic1[index]));
  }
  for (size_t index = 0; index < expectedIntrinsic0.size(); ++index) {
    CHECK(loaded.value.cameras[0].intrinsic[index] == doctest::Approx(expectedIntrinsic0[index]));
    CHECK(loaded.value.cameras[1].intrinsic[index] == doctest::Approx(expectedIntrinsic1[index]));
  }

  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

TEST_CASE("LoadCameraSet rejects malformed matrices") {
  const std::filesystem::path dir = MakeTempDir("directxsplat_facade_bad_camera_json");
  const std::filesystem::path path = dir / "cameras.json";
  WriteFile(path,
            "[{"
            "\"extrinsic\":[[1]],"
            "\"intrinsic\":[[1,0,0],[0,1,0],[0,0,1]],"
            "\"width\":1,"
            "\"height\":1"
            "}]");

  const auto loaded = dxsplat::LoadCameraSet(path);

  CHECK_FALSE(loaded.ok());
  CHECK(loaded.status.message.find("invalid camera matrix") != std::string::npos);

  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

TEST_CASE("LoadCameraSet reads DirectXSplat camera json") {
  const std::filesystem::path dir = MakeTempDir("directxsplat_facade_direct_camera_json");
  const std::filesystem::path path = dir / "cameras.json";
  WriteFile(path,
            "[{"
            "\"name\":\"direct camera\","
            "\"position\":[1,2,3],"
            "\"rotation\":[0,0,0,1],"
            "\"fovY\":1.0471975512"
            "}]");

  const auto loaded = dxsplat::LoadCameraSet(path);

  REQUIRE(loaded.ok());
  REQUIRE(loaded.value.cameras.size() == 1u);
  const dxsplat::CameraParams& camera = loaded.value.cameras.front();
  CHECK(camera.name == "direct camera");
  CHECK(camera.width == 1600);
  CHECK(camera.height == 900);
  CHECK(camera.extrinsic[0] == doctest::Approx(1.0f));
  CHECK(camera.extrinsic[3] == doctest::Approx(-1.0f));
  CHECK(camera.extrinsic[5] == doctest::Approx(-1.0f));
  CHECK(camera.extrinsic[7] == doctest::Approx(2.0f));
  CHECK(camera.extrinsic[10] == doctest::Approx(1.0f));
  CHECK(camera.extrinsic[11] == doctest::Approx(-3.0f));
  CHECK(camera.intrinsic[0] == doctest::Approx(camera.intrinsic[4]));
  CHECK(camera.intrinsic[2] == doctest::Approx(800.0f));
  CHECK(camera.intrinsic[5] == doctest::Approx(450.0f));
  CHECK(camera.intrinsic[8] == doctest::Approx(1.0f));

  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

TEST_CASE("MakeOrbitCameraSet returns empty for empty splats") {
  const dxsplat::GaussianSplats splats;
  const dxsplat::CameraSet cameras = dxsplat::MakeOrbitCameraSet(splats, 4, 1600, 900);

  CHECK(cameras.cameras.empty());
}
