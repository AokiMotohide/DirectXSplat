add_executable(DirectXSplatPlatformTests
  DirectXSplatPlatformTests.cpp
)

target_link_libraries(DirectXSplatPlatformTests PRIVATE DirectXSplat::DirectXSplat doctest::doctest)
target_include_directories(DirectXSplatPlatformTests PRIVATE ${PROJECT_SOURCE_DIR}/directxsplat/src)
target_compile_features(DirectXSplatPlatformTests PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(DirectXSplatPlatformTests)
endif()

directxsplat_add_test_executable(DirectXSplatPlatformTests "platform;core")
