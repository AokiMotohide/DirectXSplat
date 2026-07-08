add_executable(DirectXSplatRuntimeTests
  DrawConvenienceTests.cpp
  ConvenienceLoaderTests.cpp
  PublicAdvancedTypesTests.cpp
  PublicConvenienceTypesTests.cpp
  RuntimeConvenienceTests.cpp
  ViewerConvenienceContractTests.cpp
)

target_link_libraries(DirectXSplatRuntimeTests PRIVATE DirectXSplat::DirectXSplat doctest::doctest)
target_compile_definitions(DirectXSplatRuntimeTests PRIVATE
  DXSPLAT_ENABLE_GPU_TESTS=$<BOOL:${DIRECTXSPLAT_ENABLE_GPU_TESTS}>
)
target_compile_features(DirectXSplatRuntimeTests PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(DirectXSplatRuntimeTests)
endif()

directxsplat_add_test_executable(DirectXSplatRuntimeTests "runtime;convenience")

if(COMMAND directxsplat_stage_dxc_runtime)
  directxsplat_stage_dxc_runtime(DirectXSplatRuntimeTests)
endif()

add_test(
  NAME DirectXSplatInstallConsumerSmoke
  COMMAND ${CMAKE_COMMAND}
    "-DDIRECTXSPLAT_BUILD_DIR=${PROJECT_BINARY_DIR}"
    "-DDIRECTXSPLAT_CONFIG=$<CONFIG>"
    "-DDIRECTXSPLAT_GENERATOR=${CMAKE_GENERATOR}"
    "-DDIRECTXSPLAT_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    -P ${PROJECT_SOURCE_DIR}/tests/InstallConsumerSmoke.cmake
)
set_tests_properties(DirectXSplatInstallConsumerSmoke PROPERTIES LABELS "install;export;runtime")
