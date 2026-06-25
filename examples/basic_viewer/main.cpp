#include <dxsplat/directxsplat.h>

int main(int argc, char** argv) {
  if (argc != 2) {
    return 1;
  }

  auto splats = directxsplat::LoadFromPly(argv[1]);
  if (!splats.ok()) {
    return 1;
  }

  return directxsplat::Show(splats.value).ok ? 0 : 1;
}
