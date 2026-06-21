#include <dxsplat/directxsplat.h>

int main() {
  dxsplat::ViewerConfig config{};
  config.initialScenePath = "scene.spz";
  config.sceneFolderPath = "scenes";
  config.width = 1920;
  config.height = 1080;
  config.vsync = true;

  return dxsplat::Show(config).ok ? 0 : 1;
}
