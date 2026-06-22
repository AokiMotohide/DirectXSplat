add_executable(DirectXSplatCoreTests
  DiagnosticReferenceTests.cpp
  LayoutInvariantTests.cpp
  LoaderMatrixTests.cpp
  SceneIoTests.cpp
  SortTests.cpp
)

if(DXSPLAT_ENABLE_GPU_TESTS)
  target_sources(DirectXSplatCoreTests PRIVATE RendererTests.cpp)
else()
  message(STATUS "D3D12/WARP renderer tests are disabled because DXSPLAT_ENABLE_GPU_TESTS=OFF")
endif()

target_link_libraries(DirectXSplatCoreTests PRIVATE DirectXSplat::DirectXSplat doctest::doctest)
target_include_directories(DirectXSplatCoreTests
  PRIVATE
    ${PROJECT_SOURCE_DIR}/directxsplat/src
    ${PROJECT_SOURCE_DIR}/directxsplat/src/core
    ${PROJECT_SOURCE_DIR}/directxsplat/src/core/internal_headers
)
target_compile_definitions(DirectXSplatCoreTests PRIVATE DIRECTXSPLAT_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(DirectXSplatCoreTests PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(DirectXSplatCoreTests)
endif()

directxsplat_add_test_executable(DirectXSplatCoreTests "core")

if(COMMAND directxsplat_stage_dxc_runtime)
  directxsplat_stage_dxc_runtime(DirectXSplatCoreTests)
endif()
