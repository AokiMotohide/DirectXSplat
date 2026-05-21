if(NOT WIN32)
  message(STATUS "imgui_viewer skipped: requires WIN32")
  return()
endif()

if(NOT TARGET imgui)
  message(STATUS "imgui_viewer skipped: imgui target not available")
  return()
endif()

file(GLOB_RECURSE IMGUI_VIEWER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(imgui_viewer WIN32 ${IMGUI_VIEWER_SOURCES})

set_target_properties(imgui_viewer PROPERTIES OUTPUT_NAME DirectXSplatImGuiViewer)

target_include_directories(imgui_viewer
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(imgui_viewer)
endif()

target_link_libraries(imgui_viewer
  PRIVATE
    app_common
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
  directxsplat_stage_dxc_runtime(imgui_viewer)
endif()
