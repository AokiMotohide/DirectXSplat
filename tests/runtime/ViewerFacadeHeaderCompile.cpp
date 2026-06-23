#include <dxsplat/directxsplat.h>

#ifdef D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING
#error directxsplat.h must not include d3d12.h
#endif

int main() {
  dxsplat::GaussianSplats splats;
  dxsplat::CameraSet cameras;
  auto setSplats = &dxsplat::Viewer::SetSplats;
  auto setCameras = &dxsplat::Viewer::SetCameras;
  auto showSplats = static_cast<dxsplat::Status (*)(const dxsplat::GaussianSplats&, const dxsplat::ViewerConfig&)>(&dxsplat::Show);
  auto showCameras = static_cast<dxsplat::Status (*)(
      const dxsplat::GaussianSplats&, const dxsplat::CameraSet&, const dxsplat::ViewerConfig&)>(&dxsplat::Show);
  (void)cameras;
  (void)setSplats;
  (void)setCameras;
  (void)showSplats;
  (void)showCameras;
  return splats.Empty() ? 0 : 1;
}
