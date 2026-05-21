add_subdirectory(directxsplat)

if(DXSPLAT_BUILD_APPCOMMON)
  add_subdirectory(appcommon)
endif()

if(DXSPLAT_BUILD_APPCOMMON)
  add_subdirectory(imgui_viewer)
endif()

if(TARGET directxsplat_examples_common)
  add_subdirectory(examples)
endif()
