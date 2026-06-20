add_executable(ImGuiViewerTests
  ImGuiViewerCoreTests.cpp
  ScreenshotWriterTests.cpp
  ${PROJECT_SOURCE_DIR}/examples/imgui_viewer/src/metrics/ImageMetrics.cpp
  ${PROJECT_SOURCE_DIR}/examples/imgui_viewer/src/tools/CliOptions.cpp
  ${PROJECT_SOURCE_DIR}/examples/imgui_viewer/src/tools/ScreenshotWriter.cpp
)

target_include_directories(ImGuiViewerTests
  PRIVATE
    ${PROJECT_SOURCE_DIR}/examples/imgui_viewer/src
    ${PROJECT_SOURCE_DIR}/directxsplat/src
)
target_link_libraries(ImGuiViewerTests PRIVATE DirectXSplat::DirectXSplat doctest::doctest)
target_compile_features(ImGuiViewerTests PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(ImGuiViewerTests)
endif()

directxsplat_add_test_executable(ImGuiViewerTests "imguiviewer;core")
