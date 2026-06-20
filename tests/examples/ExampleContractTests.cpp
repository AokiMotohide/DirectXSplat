#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "dxsplat_examples/ExampleArgs.h"
#include "dxsplat_examples/ExampleD3D12.h"
#include "dxsplat_examples/ExampleRender.h"

namespace dxsplat::examples {
namespace {

TEST_CASE("minimal_viewer example arguments describe a windowed render loop") {
  StatusOr<ExampleOptions> parsed = ParseExampleOptions(
      ExampleKind::MinimalViewer,
      {"scene.ply", "--render-size", "640x480", "--frames", "3", "--no-vsync"});
  REQUIRE(parsed.ok());
  CHECK(parsed.value.scenePath == "scene.ply");
  CHECK(parsed.value.width == 640);
  CHECK(parsed.value.height == 480);
  CHECK(parsed.value.frameCount == 3);
  CHECK_FALSE(parsed.value.vsync);
}

TEST_CASE("offscreen_capture example arguments require a scene and provide output defaults") {
  StatusOr<ExampleOptions> parsed = ParseExampleOptions(ExampleKind::OffscreenCapture, {"scene.ply"});
  REQUIRE(parsed.ok());
  CHECK(parsed.value.outputPath == "directxsplat_capture.ppm");

  StatusOr<ExampleOptions> missing = ParseExampleOptions(ExampleKind::OffscreenCapture, {});
  CHECK_FALSE(missing.ok());
}

TEST_CASE("external_d3d12_integration example accepts host-render sizing") {
  StatusOr<ExampleOptions> parsed = ParseExampleOptions(
      ExampleKind::ExternalD3D12Integration,
      {"scene.ply", "--render-size", "320x240", "--warp"});
  REQUIRE(parsed.ok());
  CHECK(parsed.value.width == 320);
  CHECK(parsed.value.height == 240);
  CHECK(parsed.value.forceWarp);
  CHECK(ExampleUsage(ExampleKind::ExternalD3D12Integration).find("external_d3d12_integration") != std::string::npos);
}

TEST_CASE("scene_updates example arguments expose deterministic capture output") {
  StatusOr<ExampleOptions> parsed = ParseExampleOptions(
      ExampleKind::SceneUpdates,
      {"scene.ply", "--output", "after_updates.ppm"});
  REQUIRE(parsed.ok());
  CHECK(parsed.value.outputPath == "after_updates.ppm");
}

TEST_CASE("gpu_resource_interop example rejects unknown options") {
  StatusOr<ExampleOptions> parsed = ParseExampleOptions(
      ExampleKind::GpuResourceInterop,
      {"scene.ply", "--bad-option"});
  CHECK_FALSE(parsed.ok());
}

TEST_CASE("example render input is deterministic for simple scenes") {
  Scene scene{};
  scene.sceneBounds.min = {-1.0f, -1.0f, -1.0f};
  scene.sceneBounds.max = {1.0f, 1.0f, 1.0f};
  scene.sceneBounds.valid = true;

  RenderInput input = MakeExampleRenderInput(scene, 800, 400, 9);
  CHECK(input.viewportWidth == 800);
  CHECK(input.viewportHeight == 400);
  CHECK(input.frameIndex == 9);
  CHECK(input.cameraPosition.z < 0.0f);
  CHECK(input.settings.antialiasing);
  CHECK(input.settings.fastCulling);
}

TEST_CASE("empty scene copy keeps upload format metadata without chunks") {
  Scene source{};
  source.sourcePath = "source.ply";
  source.sceneBounds.valid = true;
  source.vramFormat.rgbaFormat = VramAttributeFormat::Float16;
  source.vramFormat.shFormat = VramAttributeFormat::Uint8;
  source.splatSets.push_back({});

  Scene empty = MakeSceneWithNoChunks(source);
  CHECK(empty.sourcePath == "source.ply");
  CHECK(empty.sceneBounds.valid);
  CHECK(empty.vramFormat.rgbaFormat == VramAttributeFormat::Float16);
  CHECK(empty.vramFormat.shFormat == VramAttributeFormat::Uint8);
  CHECK(empty.splatSets.empty());
}

TEST_CASE("example readback rejects malformed target footprints") {
  D3D12ExampleDevice device;
  internal::ImageRgba8 image;
  OffscreenTarget target{};
  target.width = 4;
  target.height = 4;
  target.footprint.Footprint.RowPitch = 8;
  target.readbackSizeBytes = 64;
  CHECK_FALSE(device.ReadbackImage(target, image).ok);
  CHECK_FALSE(device.RecordReadback(target).ok);
  CHECK(image.pixels.empty());

  target.footprint.Footprint.RowPitch = 16;
  target.readbackSizeBytes = 63;
  CHECK_FALSE(device.ReadbackImage(target, image).ok);
  CHECK_FALSE(device.RecordReadback(target).ok);
  CHECK(image.pixels.empty());

  target.width = UINT32_MAX;
  target.height = UINT32_MAX;
  target.footprint.Footprint.RowPitch = UINT32_MAX;
  target.readbackSizeBytes = UINT64_MAX;
  CHECK_FALSE(device.ReadbackImage(target, image).ok);
  CHECK_FALSE(device.RecordReadback(target).ok);
  CHECK(image.pixels.empty());
}

}  // namespace
}  // namespace dxsplat::examples
