add_executable(external_d3d12_integration
  src/main.cpp
)

set_target_properties(external_d3d12_integration PROPERTIES OUTPUT_NAME DirectXSplatExternalD3D12Integration)

target_link_libraries(external_d3d12_integration
  PRIVATE
    directxsplat_examples_common
)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(external_d3d12_integration)
endif()

if(COMMAND directxsplat_stage_dxc_runtime)
  directxsplat_stage_dxc_runtime(external_d3d12_integration)
endif()
