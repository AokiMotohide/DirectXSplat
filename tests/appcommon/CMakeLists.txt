add_executable(AppCommonTests
  AppCommonTests.cpp
)

target_link_libraries(AppCommonTests PRIVATE app_common doctest::doctest)
target_compile_features(AppCommonTests PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(AppCommonTests)
endif()

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(AppCommonTests DISCOVERY_TIMEOUT 120 DISCOVERY_MODE PRE_TEST PROPERTIES LABELS "appcommon;core")
