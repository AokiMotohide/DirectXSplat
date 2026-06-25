#include <directxsplat/directxsplat.h>

int main(int argc, char** argv) {
  if (argc != 2) {
    return 1;
  }

  auto splats = directxsplat::LoadFromPly(argv[1]);
  if (!splats.ok()) {
    return 1;
  }

  directxsplat::CameraSet cameras = directxsplat::MakeOrbitCameraSet(splats.value, 1, 512, 512);
  if (cameras.cameras.empty()) {
    return 1;
  }

  directxsplat::DrawOptions options{};
  options.width = 512;
  options.height = 512;

  auto image = directxsplat::Draw(splats.value, cameras.cameras[0], options);
  return image.ok() && image.value.pixels.size() == 512u * 512u * 4u ? 0 : 1;
}
