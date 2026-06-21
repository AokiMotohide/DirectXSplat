#include <dxsplat/directxsplat.h>

int main(int argc, char** argv) {
  if (argc != 2) {
    return 1;
  }

  return dxsplat::Show(argv[1]).ok ? 0 : 1;
}
