add_executable(scene_updates
  src/main.cpp
)

set_target_properties(scene_updates PROPERTIES OUTPUT_NAME DirectXSplatSceneUpdates)

target_link_libraries(scene_updates
  PRIVATE
    directxsplat_examples_common
)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(scene_updates)
endif()

if(COMMAND directxsplat_stage_dxc_runtime)
  directxsplat_stage_dxc_runtime(scene_updates)
endif()
