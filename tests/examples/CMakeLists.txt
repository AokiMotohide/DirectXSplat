add_executable(ExampleTests
  ExampleContractTests.cpp
)

target_link_libraries(ExampleTests PRIVATE directxsplat_examples_common doctest::doctest)
target_compile_features(ExampleTests PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(ExampleTests)
endif()

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(ExampleTests DISCOVERY_TIMEOUT 120 DISCOVERY_MODE PRE_TEST PROPERTIES LABELS "examples;core")
