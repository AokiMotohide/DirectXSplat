if(NOT DEFINED DIRECTXSPLAT_ROOT_DIR)
  message(FATAL_ERROR "DIRECTXSPLAT_ROOT_DIR is required")
endif()

set(required_files
  CMakeLists.txt
  LICENSE
  README.md
  directxsplat/include/directxsplat/bounding.h
  directxsplat/include/directxsplat/context.h
  directxsplat/include/directxsplat/directxsplat.h
  directxsplat/include/directxsplat/extensions.h
  directxsplat/include/directxsplat/gpu_resources.h
  directxsplat/include/directxsplat/io.h
  directxsplat/include/directxsplat/math.h
  directxsplat/include/directxsplat/render_hooks.h
  directxsplat/include/directxsplat/renderer.h
  directxsplat/include/directxsplat/scene.h
  directxsplat/include/directxsplat/settings.h
  directxsplat/include/directxsplat/sort.h
  directxsplat/include/directxsplat/status.h
  directxsplat/include/directxsplat/types.h
  directxsplat/include/directxsplat/upscaling.h
  directxsplat/include/directxsplat/vram_format.h
  docs/migration-v0.2.md
  examples/CMakeLists.txt
  examples/common/d3d12_example_common.cpp
  examples/common/d3d12_example_common.h
  examples/host_d3d12_render/main.cpp
  examples/offscreen_capture/main.cpp
  examples/scene_updates/main.cpp
  examples/gpu_resource_interop/main.cpp
  examples/basic_viewer/main.cpp
  examples/camera_viewer/main.cpp
  examples/basic_draw/main.cpp
  tests/InstallConsumerSmoke.cmake
)

foreach(path IN LISTS required_files)
  if(NOT EXISTS "${DIRECTXSPLAT_ROOT_DIR}/${path}")
    message(FATAL_ERROR "Missing required file: ${path}")
  endif()
endforeach()

set(required_dirs
  directxsplat/src/api
  directxsplat/src/app
  directxsplat/src/core
  directxsplat/src/platform
  directxsplat/src/shaders
  tests/core
  tests/platform
  tests/runtime
  tests/viewer
)

foreach(path IN LISTS required_dirs)
  if(NOT IS_DIRECTORY "${DIRECTXSPLAT_ROOT_DIR}/${path}")
    message(FATAL_ERROR "Missing required directory: ${path}")
  endif()
endforeach()

file(GLOB public_headers RELATIVE "${DIRECTXSPLAT_ROOT_DIR}/directxsplat/include/directxsplat"
  "${DIRECTXSPLAT_ROOT_DIR}/directxsplat/include/directxsplat/*.h"
)
list(SORT public_headers)
set(expected_headers
  bounding.h
  context.h
  directxsplat.h
  extensions.h
  gpu_resources.h
  io.h
  math.h
  render_hooks.h
  renderer.h
  scene.h
  settings.h
  sort.h
  status.h
  types.h
  upscaling.h
  vram_format.h
)
if(NOT public_headers STREQUAL expected_headers)
  message(FATAL_ERROR "Unexpected public headers: ${public_headers}")
endif()

file(GLOB private_advanced_headers RELATIVE "${DIRECTXSPLAT_ROOT_DIR}/directxsplat/src/core/internal_headers/directxsplat"
  "${DIRECTXSPLAT_ROOT_DIR}/directxsplat/src/core/internal_headers/directxsplat/*.h"
)
if(private_advanced_headers)
  message(FATAL_ERROR "Advanced headers should be public, not private: ${private_advanced_headers}")
endif()

set(example_sources
  examples/common/d3d12_example_common.cpp
  examples/common/d3d12_example_common.h
  examples/host_d3d12_render/main.cpp
  examples/offscreen_capture/main.cpp
  examples/scene_updates/main.cpp
  examples/gpu_resource_interop/main.cpp
  examples/basic_viewer/main.cpp
  examples/camera_viewer/main.cpp
  examples/basic_draw/main.cpp
)
string(CONCAT old_namespace "dx" "splat::")
string(CONCAT old_include_angle "#include <dx" "splat/")
string(CONCAT old_include_quote "#include \"dx" "splat/")
string(CONCAT stale_product_a "Splat" "Stream")
string(CONCAT stale_product_b "reference" "-viewer")
string(CONCAT stale_product_c "Reference" "Viewer")
set(forbidden_example_text
  "${old_namespace}"
  "${old_include_angle}"
  "${old_include_quote}"
  "${stale_product_a}"
  "${stale_product_b}"
  "${stale_product_c}"
  "src/core/internal_headers"
  "directxsplat/src/core"
  "#include \"directxsplat/"
)

foreach(path IN LISTS example_sources)
  file(READ "${DIRECTXSPLAT_ROOT_DIR}/${path}" contents)
  foreach(pattern IN LISTS forbidden_example_text)
    string(FIND "${contents}" "${pattern}" match_index)
    if(NOT match_index EQUAL -1)
      message(FATAL_ERROR "Example contains forbidden text: ${path}: ${pattern}")
    endif()
  endforeach()
endforeach()

set(convenience_example_sources
  examples/basic_viewer/main.cpp
  examples/camera_viewer/main.cpp
  examples/basic_draw/main.cpp
)
set(forbidden_convenience_text
  "d3d12.h"
  "dxgi.h"
  "directxsplat/renderer.h"
  "directxsplat/context.h"
  "directxsplat/gpu_resources.h"
  "directxsplat/io.h"
)

foreach(path IN LISTS convenience_example_sources)
  file(READ "${DIRECTXSPLAT_ROOT_DIR}/${path}" contents)
  foreach(pattern IN LISTS forbidden_convenience_text)
    string(FIND "${contents}" "${pattern}" match_index)
    if(NOT match_index EQUAL -1)
      message(FATAL_ERROR "Convenience example contains advanced include text: ${path}: ${pattern}")
    endif()
  endforeach()
endforeach()

set(advanced_example_sources
  examples/host_d3d12_render/main.cpp
  examples/offscreen_capture/main.cpp
  examples/scene_updates/main.cpp
  examples/gpu_resource_interop/main.cpp
)
set(forbidden_advanced_text
  "directxsplat::Draw("
  "directxsplat::Show("
)

foreach(path IN LISTS advanced_example_sources)
  file(READ "${DIRECTXSPLAT_ROOT_DIR}/${path}" contents)
  foreach(pattern IN LISTS forbidden_advanced_text)
    string(FIND "${contents}" "${pattern}" match_index)
    if(NOT match_index EQUAL -1)
      message(FATAL_ERROR "Advanced example contains convenience render call: ${path}: ${pattern}")
    endif()
  endforeach()
endforeach()

file(READ "${DIRECTXSPLAT_ROOT_DIR}/examples/CMakeLists.txt" examples_cmake)
set(example_targets
  DirectXSplatHostD3D12RenderExample
  DirectXSplatOffscreenCaptureExample
  DirectXSplatSceneUpdatesExample
  DirectXSplatGpuResourceInteropExample
  DirectXSplatBasicViewerExample
  DirectXSplatCameraViewerExample
  DirectXSplatBasicDrawExample
)

foreach(target IN LISTS example_targets)
  string(FIND "${examples_cmake}" "${target}" match_index)
  if(match_index EQUAL -1)
    message(FATAL_ERROR "Missing example CMake target: ${target}")
  endif()
endforeach()

set(removed_paths
  appcommon
  shaders
  examples/minimal_viewer
  examples/external_d3d12_integration
  tests/directxsplat
  tests/examples
)

foreach(path IN LISTS removed_paths)
  if(EXISTS "${DIRECTXSPLAT_ROOT_DIR}/${path}")
    message(FATAL_ERROR "Removed path is still present: ${path}")
  endif()
endforeach()
