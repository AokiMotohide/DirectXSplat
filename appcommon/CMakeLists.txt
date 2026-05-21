add_library(app_common
  src/swapchain_context.cpp
  src/image.cpp
)

target_include_directories(app_common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(app_common PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(app_common)
endif()

target_link_libraries(app_common
  PUBLIC
    DirectXSplat::DirectXSplat
    dxgi
    dxguid
    d3d12
)
