add_executable(AppCommonTests
  AppCommonTests.cpp
)

target_link_libraries(AppCommonTests PRIVATE app_common doctest::doctest)
target_compile_features(AppCommonTests PRIVATE cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(AppCommonTests)
endif()

directxsplat_add_test_executable(AppCommonTests "appcommon;core")
