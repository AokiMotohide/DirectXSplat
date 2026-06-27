add_executable(DirectXSplatBench
  directxsplat_bench.cpp
)

target_compile_features(DirectXSplatBench PRIVATE cxx_std_20)

target_include_directories(DirectXSplatBench
  PRIVATE
    ${PROJECT_SOURCE_DIR}/directxsplat/src
    ${PROJECT_SOURCE_DIR}/directxsplat/src/api
    ${PROJECT_SOURCE_DIR}/directxsplat/src/core
    ${PROJECT_SOURCE_DIR}/directxsplat/src/core/internal_headers
)

target_link_libraries(DirectXSplatBench PRIVATE DirectXSplat::DirectXSplat)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(DirectXSplatBench)
endif()

if(COMMAND directxsplat_stage_dxc_runtime)
  directxsplat_stage_dxc_runtime(DirectXSplatBench)
endif()
