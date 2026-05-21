add_executable(ImGuiViewerTests
  ImGuiViewerCoreTests.cpp
  ScreenshotWriterTests.cpp
  ${PROJECT_SOURCE_DIR}/examples/imgui_viewer/src/metrics/ImageMetrics.cpp
  ${PROJECT_SOURCE_DIR}/examples/imgui_viewer/src/tools/CliOptions.cpp
  ${PROJECT_SOURCE_DIR}/examples/imgui_viewer/src/tools/ScreenshotWriter.cpp
)

target_include_directories(ImGuiViewerTests PRIVATE ${PROJECT_SOURCE_DIR}/examples/imgui_viewer/src)
target_link_libraries(ImGuiViewerTests PRIVATE app_common doctest::doctest)
target_compile_features(ImGuiViewerTests PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(ImGuiViewerTests)
endif()

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(ImGuiViewerTests DISCOVERY_TIMEOUT 120 DISCOVERY_MODE PRE_TEST PROPERTIES LABELS "imguiviewer;core")
