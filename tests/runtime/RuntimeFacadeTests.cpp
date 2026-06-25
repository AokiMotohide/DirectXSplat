#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dxsplat/directxsplat.h>

TEST_CASE("ViewerConfig rejects zero dimensions") {
  directxsplat::ViewerConfig config{};
  config.width = 0;

  directxsplat::Viewer widthViewer;
  const directxsplat::Status widthStatus = widthViewer.Initialize(config);
  CHECK_FALSE(widthStatus.ok);
  CHECK(widthStatus.message == "viewer width must be greater than zero");

  config.width = 1600;
  config.height = 0;

  directxsplat::Viewer heightViewer;
  const directxsplat::Status heightStatus = heightViewer.Initialize(config);
  CHECK_FALSE(heightStatus.ok);
  CHECK(heightStatus.message == "viewer height must be greater than zero");
}

TEST_CASE("Viewer cannot load before initialization") {
  directxsplat::Viewer viewer;
  const directxsplat::Status status = viewer.Load("scene.ply");

  CHECK_FALSE(status.ok);
  CHECK(status.message == "viewer is not initialized");
}
