add_executable(minimal_viewer
  src/main.cpp
)

set_target_properties(minimal_viewer PROPERTIES OUTPUT_NAME DirectXSplatMinimalViewer)

target_link_libraries(minimal_viewer
  PRIVATE
    directxsplat_examples_common
    user32
)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(minimal_viewer)
endif()

if(COMMAND directxsplat_stage_dxc_runtime)
  directxsplat_stage_dxc_runtime(minimal_viewer)
endif()
