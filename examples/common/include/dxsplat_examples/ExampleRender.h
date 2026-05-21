#pragma once

#include <string>
#include <vector>

#include "dxsplat/io.h"
#include "dxsplat/renderer.h"
#include "dxsplat/scene.h"
#include "dxsplat/settings.h"
#include "dxsplat/status.h"
#include "dxsplat/types.h"
#include "dxsplat_examples/ExampleD3D12.h"

namespace dxsplat::examples {

StatusOr<Scene> LoadExampleScene(const std::string& path);
RenderInput MakeExampleRenderInput(const Scene& scene, uint32_t width, uint32_t height, uint64_t frameIndex);
RenderTargetBinding MakeOffscreenRenderTarget(const OffscreenTarget& target);
Status RenderSceneToOffscreen(D3D12ExampleDevice& device,
                              Renderer& renderer,
                              UploadedSceneHandle sceneHandle,
                              const RenderInput& input,
                              OffscreenTarget& target,
                              RenderResult& outResult);
Status UploadScene(Renderer& renderer,
                   const Scene& scene,
                   UploadedSceneHandle& outSceneHandle,
                   std::vector<UploadedChunkHandle>* outChunks);
Scene MakeSceneWithNoChunks(const Scene& source);

}  // namespace dxsplat::examples
