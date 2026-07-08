function(directxsplat_add_test_executable target_name labels)
  add_test(NAME ${target_name} COMMAND $<TARGET_FILE:${target_name}>)
  set_tests_properties(${target_name} PROPERTIES LABELS "${labels}")
endfunction()

add_subdirectory(core)
add_subdirectory(platform)

if(DIRECTXSPLAT_BUILD_VIEWER)
  add_subdirectory(viewer)
endif()

add_subdirectory(runtime)
