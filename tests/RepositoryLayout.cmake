if(NOT DEFINED DIRECTXSPLAT_ROOT_DIR)
  message(FATAL_ERROR "DIRECTXSPLAT_ROOT_DIR is required")
endif()

set(required_files
  CMakeLists.txt
  LICENSE
  README.md
  directxsplat/include/dxsplat/directxsplat.h
  directxsplat/include/dxsplat/status.h
  docs/migration-v0.2.md
  examples/CMakeLists.txt
  examples/basic_viewer/main.cpp
  examples/configured_viewer/main.cpp
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
  directxsplat/src/core/internal_headers
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

file(GLOB public_headers RELATIVE "${DIRECTXSPLAT_ROOT_DIR}/directxsplat/include/dxsplat"
  "${DIRECTXSPLAT_ROOT_DIR}/directxsplat/include/dxsplat/*.h"
)
list(SORT public_headers)
set(expected_headers
  directxsplat.h
  status.h
)
if(NOT public_headers STREQUAL expected_headers)
  message(FATAL_ERROR "Unexpected public headers: ${public_headers}")
endif()

set(forbidden_public_headers
  context.h
  gpu_resources.h
  io.h
  renderer.h
)
foreach(header IN LISTS forbidden_public_headers)
  if(EXISTS "${DIRECTXSPLAT_ROOT_DIR}/directxsplat/include/dxsplat/${header}")
    message(FATAL_ERROR "Removed SDK header is still public: ${header}")
  endif()
endforeach()

set(example_sources
  examples/basic_viewer/main.cpp
  examples/configured_viewer/main.cpp
)
set(forbidden_example_text
  "d3d12.h"
  "dxgi.h"
  "dxsplat/renderer.h"
  "dxsplat/context.h"
)

foreach(path IN LISTS example_sources)
  file(READ "${DIRECTXSPLAT_ROOT_DIR}/${path}" contents)
  foreach(pattern IN LISTS forbidden_example_text)
    string(FIND "${contents}" "${pattern}" match_index)
    if(NOT match_index EQUAL -1)
      message(FATAL_ERROR "Facade example contains forbidden include text: ${path}: ${pattern}")
    endif()
  endforeach()
endforeach()

set(removed_paths
  appcommon
  shaders
  examples/common
  examples/minimal_viewer
  examples/offscreen_capture
  examples/external_d3d12_integration
  examples/scene_updates
  examples/gpu_resource_interop
  tests/directxsplat
  tests/examples
)

foreach(path IN LISTS removed_paths)
  if(EXISTS "${DIRECTXSPLAT_ROOT_DIR}/${path}")
    message(FATAL_ERROR "Removed path is still present: ${path}")
  endif()
endforeach()
