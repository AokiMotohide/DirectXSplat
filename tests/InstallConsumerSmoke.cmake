if(NOT DEFINED DIRECTXSPLAT_BUILD_DIR)
  message(FATAL_ERROR "DIRECTXSPLAT_BUILD_DIR is required")
endif()
if(NOT DEFINED DIRECTXSPLAT_CONFIG)
  message(FATAL_ERROR "DIRECTXSPLAT_CONFIG is required")
endif()
if(NOT DEFINED DIRECTXSPLAT_GENERATOR)
  message(FATAL_ERROR "DIRECTXSPLAT_GENERATOR is required")
endif()

set(prefix "${DIRECTXSPLAT_BUILD_DIR}/install-consumer-smoke/${DIRECTXSPLAT_CONFIG}/prefix")
set(consumer_source "${DIRECTXSPLAT_BUILD_DIR}/install-consumer-smoke/${DIRECTXSPLAT_CONFIG}/src")
set(consumer_build "${DIRECTXSPLAT_BUILD_DIR}/install-consumer-smoke/${DIRECTXSPLAT_CONFIG}/build")

file(REMOVE_RECURSE "${prefix}" "${consumer_source}" "${consumer_build}")
file(MAKE_DIRECTORY "${consumer_source}")

file(WRITE "${consumer_source}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.25)
project(DirectXSplatInstalledConsumer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(DirectXSplat CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE DirectXSplat::DirectXSplat)
]=])

file(WRITE "${consumer_source}/main.cpp" [=[
#include <directxsplat/context.h>
#include <directxsplat/directxsplat.h>
#include <directxsplat/gpu_resources.h>
#include <directxsplat/io.h>
#include <directxsplat/renderer.h>

int main() {
  directxsplat::D3D12Context context{};
  directxsplat::Renderer renderer{};
  directxsplat::RendererConfig rendererConfig{};
  directxsplat::Scene scene{};
  directxsplat::RenderInput input{};
  directxsplat::RenderTargetBinding target{};
  directxsplat::RenderFrameContext frameContext{};
  directxsplat::RenderSubmissionInfo submission{};
  directxsplat::GpuBufferView buffer{};
  directxsplat::GpuTextureView texture{};
  directxsplat::GpuFrameResources frameResources{};
  directxsplat::UploadedSceneHandle uploadedScene{};
  directxsplat::UploadedChunkHandle uploadedChunk{};
  directxsplat::SceneMutationToken mutation{};
  directxsplat::SceneAccessInfo accessInfo{};
  directxsplat::UploadedSceneInfo sceneInfo{};
  directxsplat::UploadedChunkInfo chunkInfo{};
  directxsplat::UploadedSceneGpuResources sceneResources{};
  directxsplat::UploadedChunkGpuResources chunkResources{};
  directxsplat::UploadSyncPoint uploadSync{};
  directxsplat::DrawOptions draw{};
  directxsplat::ViewerConfig viewer{};
  viewer.width = 640;
  viewer.height = 480;

  return context.Device() == nullptr &&
                 !renderer.IsDeviceLost() &&
                 rendererConfig.maxUploadsPerFrame > 0 &&
                 scene.splatSets.empty() &&
                 input.viewportWidth == 1 &&
                 target.colorTarget == nullptr &&
                 !frameContext.HasFence() &&
                 !submission.submissionRequired &&
                 !buffer.IsValid() &&
                 !texture.IsValid() &&
                 !frameResources.colorValid &&
                 !uploadedScene.IsValid() &&
                 !uploadedChunk.IsValid() &&
                 !mutation.IsValid() &&
                 !accessInfo.readyToRender &&
                 !sceneInfo.readyToRender &&
                 !chunkInfo.resident &&
                 sceneResources.chunks.empty() &&
                 chunkResources.format == directxsplat::UploadedSceneBufferFormat::Unknown &&
                 !uploadSync.IsValid() &&
                 draw.width == 1600 &&
                 viewer.width == 640
             ? 0
             : 1;
}
]=])

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${DIRECTXSPLAT_BUILD_DIR}" --config "${DIRECTXSPLAT_CONFIG}" --prefix "${prefix}"
  RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "DirectXSplat install failed")
endif()

set(configure_command
  "${CMAKE_COMMAND}"
  -S "${consumer_source}"
  -B "${consumer_build}"
  -G "${DIRECTXSPLAT_GENERATOR}"
  "-DCMAKE_PREFIX_PATH=${prefix}"
)
if(DEFINED DIRECTXSPLAT_GENERATOR_PLATFORM AND NOT DIRECTXSPLAT_GENERATOR_PLATFORM STREQUAL "")
  list(APPEND configure_command -A "${DIRECTXSPLAT_GENERATOR_PLATFORM}")
endif()

execute_process(
  COMMAND ${configure_command}
  RESULT_VARIABLE configure_result
)
if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR "Installed DirectXSplat consumer configure failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${consumer_build}" --config "${DIRECTXSPLAT_CONFIG}" --parallel
  RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "Installed DirectXSplat consumer build failed")
endif()
