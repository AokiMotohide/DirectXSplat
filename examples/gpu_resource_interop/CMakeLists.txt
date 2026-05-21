add_executable(gpu_resource_interop
  src/main.cpp
)

set_target_properties(gpu_resource_interop PROPERTIES OUTPUT_NAME DirectXSplatGpuResourceInterop)

target_link_libraries(gpu_resource_interop
  PRIVATE
    directxsplat_examples_common
)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(gpu_resource_interop)
endif()

if(COMMAND directxsplat_stage_dxc_runtime)
  directxsplat_stage_dxc_runtime(gpu_resource_interop)
endif()
