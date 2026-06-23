add_executable(DirectXSplatRuntimeTests
  FacadeLoaderTests.cpp
  PublicFacadeTypesTests.cpp
  RuntimeFacadeTests.cpp
  ViewerFacadeContractTests.cpp
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

add_executable(DirectXSplatPublicFacadeHeaderCompile
  PublicFacadeHeaderCompile.cpp
)

target_link_libraries(DirectXSplatPublicFacadeHeaderCompile PRIVATE DirectXSplat::DirectXSplat)
target_compile_features(DirectXSplatPublicFacadeHeaderCompile PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(DirectXSplatPublicFacadeHeaderCompile)
endif()

directxsplat_add_test_executable(DirectXSplatPublicFacadeHeaderCompile "runtime;facade")

add_executable(DirectXSplatViewerFacadeHeaderCompile
  ViewerFacadeHeaderCompile.cpp
)

target_link_libraries(DirectXSplatViewerFacadeHeaderCompile PRIVATE DirectXSplat::DirectXSplat)
target_compile_features(DirectXSplatViewerFacadeHeaderCompile PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(DirectXSplatViewerFacadeHeaderCompile)
endif()

directxsplat_add_test_executable(DirectXSplatViewerFacadeHeaderCompile "runtime;facade")

add_test(
  NAME DirectXSplatRepositoryLayout
  COMMAND ${CMAKE_COMMAND}
    -DDIRECTXSPLAT_ROOT_DIR=${PROJECT_SOURCE_DIR}
    -P ${PROJECT_SOURCE_DIR}/tests/RepositoryLayout.cmake
)
set_tests_properties(DirectXSplatRepositoryLayout PROPERTIES LABELS "repository;layout")
