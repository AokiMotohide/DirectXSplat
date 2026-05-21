add_executable(offscreen_capture
  src/main.cpp
)

set_target_properties(offscreen_capture PROPERTIES OUTPUT_NAME DirectXSplatOffscreenCapture)

target_link_libraries(offscreen_capture
  PRIVATE
    directxsplat_examples_common
)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(offscreen_capture)
endif()

if(COMMAND directxsplat_stage_dxc_runtime)
  directxsplat_stage_dxc_runtime(offscreen_capture)
endif()
