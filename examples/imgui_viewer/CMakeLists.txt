if(NOT WIN32)
  message(STATUS "directxsplat_viewer skipped: requires WIN32")
  return()
endif()

if(NOT TARGET imgui)
  message(STATUS "directxsplat_viewer skipped: imgui target not available")
  return()
endif()

file(GLOB_RECURSE IMGUI_VIEWER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(directxsplat_viewer WIN32 ${IMGUI_VIEWER_SOURCES})

set_target_properties(directxsplat_viewer PROPERTIES OUTPUT_NAME DirectXSplatViewer)

target_include_directories(directxsplat_viewer
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/directxsplat/src
)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(directxsplat_viewer)
endif()

target_link_libraries(directxsplat_viewer
  PRIVATE
    DirectXSplat::DirectXSplat
    imgui
    d3d12
    dxgi
    dxguid
    d3dcompiler
    dwmapi
    shell32
    ole32
)

if(COMMAND directxsplat_stage_dxc_runtime)
  directxsplat_stage_dxc_runtime(directxsplat_viewer)
endif()
