#include <directxsplat/directxsplat.h>

int main(int argc, char** argv) {
  if (argc != 3) {
    return 1;
  }

  auto splats = directxsplat::LoadFromPly(argv[1]);
  if (!splats.ok()) {
    return 1;
  }

  auto cameras = directxsplat::LoadCameraSet(argv[2]);
  if (!cameras.ok()) {
    return 1;
  }

  return directxsplat::Show(splats.value, cameras.value).ok ? 0 : 1;
}
