#include <dxsplat/directxsplat.h>

#ifdef D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING
#error directxsplat.h must not include d3d12.h
#endif

int main() {
  dxsplat::GaussianSplats splats;
  dxsplat::DrawOptions options;
  return splats.Empty() && options.width == 1600 ? 0 : 1;
}
