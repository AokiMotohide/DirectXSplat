cmake_minimum_required(VERSION 3.25)

project(DirectXSplat VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MSVC)
  message(FATAL_ERROR "DirectXSplat currently requires MSVC + Windows SDK (use -G \"Visual Studio 17 2022\" -A x64).")
endif()

function(directxsplat_bool_option name legacy_name description default_value)
  if(DEFINED ${legacy_name} AND NOT DEFINED ${name})
    set(${name} "${${legacy_name}}" CACHE BOOL "${description}")
  else()
    option(${name} "${description}" "${default_value}")
  endif()
  set(${legacy_name} "${${name}}" CACHE BOOL "${description}" FORCE)
endfunction()

if(PROJECT_IS_TOP_LEVEL)
  set(DIRECTXSPLAT_DEFAULT_BUILD_TESTS ON)
  set(DIRECTXSPLAT_DEFAULT_BUILD_VIEWER ON)
  set(DIRECTXSPLAT_DEFAULT_BUILD_LEGACY_EXAMPLES OFF)
else()
  set(DIRECTXSPLAT_DEFAULT_BUILD_TESTS OFF)
  set(DIRECTXSPLAT_DEFAULT_BUILD_VIEWER OFF)
  set(DIRECTXSPLAT_DEFAULT_BUILD_LEGACY_EXAMPLES OFF)
endif()
directxsplat_bool_option(DXSPLAT_BUILD_TESTS DIRECTXSPLAT_BUILD_TESTS "Build tests" ${DIRECTXSPLAT_DEFAULT_BUILD_TESTS})
directxsplat_bool_option(DXSPLAT_BUILD_VIEWER DIRECTXSPLAT_BUILD_VIEWER "Build DirectXSplat viewer" ${DIRECTXSPLAT_DEFAULT_BUILD_VIEWER})
directxsplat_bool_option(DXSPLAT_BUILD_LEGACY_EXAMPLES DIRECTXSPLAT_BUILD_LEGACY_EXAMPLES "Build legacy examples" ${DIRECTXSPLAT_DEFAULT_BUILD_LEGACY_EXAMPLES})
directxsplat_bool_option(DXSPLAT_ENABLE_GPU_TESTS DIRECTXSPLAT_ENABLE_GPU_TESTS "Build D3D12/WARP renderer tests" ON)
directxsplat_bool_option(DXSPLAT_ENABLE_WARNINGS DIRECTXSPLAT_ENABLE_WARNINGS "Enable project compiler warnings" ON)
directxsplat_bool_option(DXSPLAT_WARNINGS_AS_ERRORS DIRECTXSPLAT_WARNINGS_AS_ERRORS "Treat project compiler warnings as errors" OFF)

function(directxsplat_apply_project_warnings target_name)
  if(NOT DXSPLAT_ENABLE_WARNINGS)
    return()
  endif()
  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4)
    if(DXSPLAT_WARNINGS_AS_ERRORS)
      target_compile_options(${target_name} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    if(DXSPLAT_WARNINGS_AS_ERRORS)
      target_compile_options(${target_name} PRIVATE -Werror)
    endif()
  endif()
endfunction()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(FetchContent)

function(directxsplat_stage_dxc_runtime target_name)
  get_property(_dxcompiler GLOBAL PROPERTY DIRECTXSPLAT_DXCOMPILER_DLL)
  get_property(_dxil GLOBAL PROPERTY DIRECTXSPLAT_DXIL_DLL)
  if(_dxcompiler AND EXISTS "${_dxcompiler}")
    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dxcompiler}" "$<TARGET_FILE_DIR:${target_name}>"
    )
  endif()
  if(_dxil AND EXISTS "${_dxil}")
    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dxil}" "$<TARGET_FILE_DIR:${target_name}>"
    )
  endif()
endfunction()

FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip
)
FetchContent_MakeAvailable(nlohmann_json)

if(DXSPLAT_BUILD_VIEWER AND WIN32 AND MSVC)
  FetchContent_Declare(
    imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.6.zip
  )
  FetchContent_MakeAvailable(imgui)

  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  )
  target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_compile_definitions(imgui PUBLIC IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)
  target_link_libraries(imgui PUBLIC d3d12 dxgi)
endif()

if(DXSPLAT_BUILD_TESTS)
  FetchContent_Declare(
    doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.zip
  )
  FetchContent_MakeAvailable(doctest)
endif()

add_subdirectory(directxsplat)

if(DXSPLAT_BUILD_LEGACY_EXAMPLES)
  add_subdirectory(examples)
endif()

if(DXSPLAT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
