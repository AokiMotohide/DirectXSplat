add_library(directxsplat_examples_common
  src/ExampleArgs.cpp
  src/ExampleD3D12.cpp
  src/ExampleRender.cpp
)

target_include_directories(directxsplat_examples_common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/directxsplat/src
)

target_compile_definitions(directxsplat_examples_common PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(directxsplat_examples_common)
endif()

target_link_libraries(directxsplat_examples_common
  PUBLIC
    DirectXSplat::DirectXSplat
    d3d12
    dxgi
    dxguid
)
