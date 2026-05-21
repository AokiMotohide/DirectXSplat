add_executable(DirectXSplatTests
  DiagnosticReferenceTests.cpp
  LayoutInvariantTests.cpp
  LoaderMatrixTests.cpp
  SceneIoTests.cpp
  SortTests.cpp
)

if(DXSPLAT_ENABLE_GPU_TESTS)
  target_sources(DirectXSplatTests PRIVATE RendererTests.cpp)
else()
  message(STATUS "D3D12/WARP renderer tests are disabled because DXSPLAT_ENABLE_GPU_TESTS=OFF")
endif()

target_link_libraries(DirectXSplatTests PRIVATE DirectXSplat::DirectXSplat doctest::doctest)
target_include_directories(DirectXSplatTests PRIVATE ${PROJECT_SOURCE_DIR}/directxsplat/src)
target_compile_definitions(DirectXSplatTests PRIVATE DIRECTXSPLAT_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(DirectXSplatTests PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(DirectXSplatTests)
endif()

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(DirectXSplatTests DISCOVERY_TIMEOUT 120 DISCOVERY_MODE PRE_TEST PROPERTIES LABELS "directxsplat;core")

if(COMMAND directxsplat_stage_dxc_runtime)
  directxsplat_stage_dxc_runtime(DirectXSplatTests)
endif()
