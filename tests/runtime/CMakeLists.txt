add_executable(DirectXSplatRuntimeTests
  DrawFacadeTests.cpp
  ExampleSourcePolicyTests.cpp
  FacadeLoaderTests.cpp
  PublicAdvancedTypesTests.cpp
  PublicFacadeTypesTests.cpp
  RuntimeFacadeTests.cpp
  ViewerFacadeContractTests.cpp
)

target_link_libraries(DirectXSplatRuntimeTests PRIVATE DirectXSplat::DirectXSplat doctest::doctest)
target_compile_definitions(DirectXSplatRuntimeTests PRIVATE
  DXSPLAT_ENABLE_GPU_TESTS=$<BOOL:${DXSPLAT_ENABLE_GPU_TESTS}>
  DIRECTXSPLAT_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/examples"
)
target_compile_features(DirectXSplatRuntimeTests PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(DirectXSplatRuntimeTests)
endif()

directxsplat_add_test_executable(DirectXSplatRuntimeTests "runtime;facade")

if(COMMAND directxsplat_stage_dxc_runtime)
  directxsplat_stage_dxc_runtime(DirectXSplatRuntimeTests)
endif()

add_test(
  NAME DirectXSplatRepositoryLayout
  COMMAND ${CMAKE_COMMAND}
    -DDIRECTXSPLAT_ROOT_DIR=${PROJECT_SOURCE_DIR}
    -P ${PROJECT_SOURCE_DIR}/tests/RepositoryLayout.cmake
)
set_tests_properties(DirectXSplatRepositoryLayout PROPERTIES LABELS "repository;layout")

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
