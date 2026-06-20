function(directxsplat_add_test_executable target_name labels)
  add_test(NAME ${target_name} COMMAND $<TARGET_FILE:${target_name}>)
  set_tests_properties(${target_name} PROPERTIES LABELS "${labels}")
endfunction()

add_subdirectory(directxsplat)
add_subdirectory(platform)
add_subdirectory(runtime)

if(DXSPLAT_BUILD_VIEWER)
  add_subdirectory(viewer)
endif()

if(TARGET directxsplat_examples_common)
  add_subdirectory(examples)
endif()
