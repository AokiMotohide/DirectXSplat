add_executable(DirectXSplatRuntimeTests
  RuntimeFacadeTests.cpp
)

target_link_libraries(DirectXSplatRuntimeTests PRIVATE DirectXSplat::DirectXSplat doctest::doctest)
target_compile_features(DirectXSplatRuntimeTests PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(DirectXSplatRuntimeTests)
endif()

directxsplat_add_test_executable(DirectXSplatRuntimeTests "runtime;facade")

if(COMMAND directxsplat_stage_dxc_runtime)
  directxsplat_stage_dxc_runtime(DirectXSplatRuntimeTests)
endif()
