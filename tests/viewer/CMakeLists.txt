add_executable(DirectXSplatViewerTests
  ImGuiViewerCoreTests.cpp
  ScreenshotWriterTests.cpp
  StatisticsGraphTests.cpp
  UiLayerTests.cpp
  ${PROJECT_SOURCE_DIR}/directxsplat/src/app/metrics/ImageMetrics.cpp
  ${PROJECT_SOURCE_DIR}/directxsplat/src/app/tools/CliOptions.cpp
  ${PROJECT_SOURCE_DIR}/directxsplat/src/app/tools/ScreenshotWriter.cpp
)

target_include_directories(DirectXSplatViewerTests
  PRIVATE
    ${PROJECT_SOURCE_DIR}/directxsplat/src/app
    ${PROJECT_SOURCE_DIR}/directxsplat/src
    ${PROJECT_SOURCE_DIR}/directxsplat/src/core/internal_headers
)
target_link_libraries(DirectXSplatViewerTests PRIVATE DirectXSplat::DirectXSplat doctest::doctest)
target_compile_features(DirectXSplatViewerTests PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(DirectXSplatViewerTests)
endif()

directxsplat_add_test_executable(DirectXSplatViewerTests "viewer;core")
