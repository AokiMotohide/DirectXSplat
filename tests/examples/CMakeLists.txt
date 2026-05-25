add_executable(ExampleTests
  ExampleContractTests.cpp
)

target_link_libraries(ExampleTests PRIVATE directxsplat_examples_common doctest::doctest)
target_compile_features(ExampleTests PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(ExampleTests)
endif()

directxsplat_add_test_executable(ExampleTests "examples;core")
