include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

get_filename_component(DIRECTXSPLAT_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(DIRECTXSPLAT_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders")
set(DIRECTXSPLAT_CMAKE_DIR "${DIRECTXSPLAT_ROOT_DIR}/cmake")
set(DIRECTXSPLAT_3RDPARTY_DIR "${DIRECTXSPLAT_ROOT_DIR}/3rdparty/GPUSorting/GPUSortingD3D12")
set(DIRECTXSPLAT_3RDPARTY_SHADER_DIR "${DIRECTXSPLAT_3RDPARTY_DIR}/Shaders")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(DIRECTXSPLAT_ARCH_DIR x64)
else()
  set(DIRECTXSPLAT_ARCH_DIR x86)
endif()

set(DIRECTXSPLAT_DXCOMPILER_DLL "")
set(DIRECTXSPLAT_DXIL_DLL "")
file(GLOB DIRECTXSPLAT_DXCOMPILER_CANDIDATES
  "C:/Program Files (x86)/Windows Kits/10/Redist/D3D/${DIRECTXSPLAT_ARCH_DIR}/dxcompiler.dll"
  "C:/Program Files (x86)/Windows Kits/10/bin/*/${DIRECTXSPLAT_ARCH_DIR}/dxcompiler.dll"
)
list(SORT DIRECTXSPLAT_DXCOMPILER_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
foreach(candidate IN LISTS DIRECTXSPLAT_DXCOMPILER_CANDIDATES)
  if(EXISTS "${candidate}")
    set(DIRECTXSPLAT_DXCOMPILER_DLL "${candidate}")
    break()
  endif()
endforeach()
file(GLOB DIRECTXSPLAT_DXIL_CANDIDATES
  "C:/Program Files (x86)/Windows Kits/10/Redist/D3D/${DIRECTXSPLAT_ARCH_DIR}/dxil.dll"
  "C:/Program Files (x86)/Windows Kits/10/bin/*/${DIRECTXSPLAT_ARCH_DIR}/dxil.dll"
)
list(SORT DIRECTXSPLAT_DXIL_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
foreach(candidate IN LISTS DIRECTXSPLAT_DXIL_CANDIDATES)
  if(EXISTS "${candidate}")
    set(DIRECTXSPLAT_DXIL_DLL "${candidate}")
    break()
  endif()
endforeach()
set_property(GLOBAL PROPERTY DIRECTXSPLAT_DXCOMPILER_DLL "${DIRECTXSPLAT_DXCOMPILER_DLL}")
set_property(GLOBAL PROPERTY DIRECTXSPLAT_DXIL_DLL "${DIRECTXSPLAT_DXIL_DLL}")

file(GLOB_RECURSE DIRECTXSPLAT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
set(DXSPLAT_APP_SOURCES
  src/app/Application.cpp
  src/app/CameraController.cpp
  src/app/platform/Win32Window.cpp
  src/app/scene/SceneManager.cpp
  src/app/tools/CliOptions.cpp
  src/app/tools/ScreenshotWriter.cpp
  src/app/ui/UiLayer.cpp
  src/app/metrics/ImageMetrics.cpp
)
set(DXSPLAT_APP_SOURCE_PATHS ${DXSPLAT_APP_SOURCES})
list(TRANSFORM DXSPLAT_APP_SOURCE_PATHS PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
list(REMOVE_ITEM DIRECTXSPLAT_SOURCES
  ${DXSPLAT_APP_SOURCE_PATHS}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app/main.cpp
)
set(DIRECTXSPLAT_THIRDPARTY_SOURCES
  ${DIRECTXSPLAT_ROOT_DIR}/3rdparty/miniz.c
  ${DIRECTXSPLAT_ROOT_DIR}/3rdparty/miniz_tdef.c
  ${DIRECTXSPLAT_ROOT_DIR}/3rdparty/miniz_tinfl.c
  ${DIRECTXSPLAT_ROOT_DIR}/3rdparty/miniz_zip.c
)
list(APPEND DIRECTXSPLAT_SOURCES ${DIRECTXSPLAT_THIRDPARTY_SOURCES})

if(MSVC)
  set_source_files_properties(${DIRECTXSPLAT_THIRDPARTY_SOURCES} PROPERTIES COMPILE_OPTIONS /W0)
endif()

set(DIRECTXSPLAT_EMBEDDED_SHADER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedShaders.h)
add_custom_command(
  OUTPUT ${DIRECTXSPLAT_EMBEDDED_SHADER_HEADER}
  COMMAND ${CMAKE_COMMAND}
    -DOUTPUT=${DIRECTXSPLAT_EMBEDDED_SHADER_HEADER}
    -DCOMPUTE_INPUT=${DIRECTXSPLAT_SHADER_DIR}/gaussian_compute.hlsl
    -DRASTER_INPUT=${DIRECTXSPLAT_SHADER_DIR}/gaussian_raster.hlsl
    -DONESWEEP_INPUT=${DIRECTXSPLAT_3RDPARTY_SHADER_DIR}/OneSweep.hlsl
    -DSWEEP_COMMON_INPUT=${DIRECTXSPLAT_3RDPARTY_SHADER_DIR}/SweepCommon.hlsl
    -DSORT_COMMON_INPUT=${DIRECTXSPLAT_3RDPARTY_SHADER_DIR}/SortCommon.hlsl
    -P ${DIRECTXSPLAT_CMAKE_DIR}/GenerateEmbeddedShaders.cmake
  DEPENDS
    ${DIRECTXSPLAT_CMAKE_DIR}/GenerateEmbeddedShaders.cmake
    ${DIRECTXSPLAT_SHADER_DIR}/gaussian_compute.hlsl
    ${DIRECTXSPLAT_SHADER_DIR}/gaussian_raster.hlsl
    ${DIRECTXSPLAT_3RDPARTY_SHADER_DIR}/OneSweep.hlsl
    ${DIRECTXSPLAT_3RDPARTY_SHADER_DIR}/SweepCommon.hlsl
    ${DIRECTXSPLAT_3RDPARTY_SHADER_DIR}/SortCommon.hlsl
)

list(APPEND DIRECTXSPLAT_SOURCES ${DIRECTXSPLAT_EMBEDDED_SHADER_HEADER})

add_library(dxsplat STATIC ${DIRECTXSPLAT_SOURCES})
add_library(dxsplat::dxsplat ALIAS dxsplat)
add_library(DirectXSplat::DirectXSplat ALIAS dxsplat)

set_target_properties(dxsplat PROPERTIES
  OUTPUT_NAME DirectXSplat
  EXPORT_NAME DirectXSplat
)

target_compile_features(dxsplat PUBLIC cxx_std_20)

if(COMMAND directxsplat_apply_project_warnings)
  directxsplat_apply_project_warnings(dxsplat)
endif()

target_include_directories(dxsplat
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_include_directories(dxsplat SYSTEM
  PRIVATE
    ${DIRECTXSPLAT_ROOT_DIR}/3rdparty
    ${DIRECTXSPLAT_3RDPARTY_DIR}
)

if(NOT TARGET nlohmann_json::nlohmann_json)
  find_package(nlohmann_json CONFIG REQUIRED)
endif()

get_target_property(DIRECTXSPLAT_JSON_INCLUDE_DIRS
  nlohmann_json::nlohmann_json
  INTERFACE_INCLUDE_DIRECTORIES)
if(DIRECTXSPLAT_JSON_INCLUDE_DIRS)
  target_include_directories(dxsplat SYSTEM PRIVATE ${DIRECTXSPLAT_JSON_INCLUDE_DIRS})
endif()

target_compile_definitions(dxsplat
  PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
  PUBLIC
    NOMINMAX
)

target_link_libraries(dxsplat
  PUBLIC
    d3d12
    dxgi
    dxguid
    d3dcompiler
    dxcompiler
    windowscodecs
    ole32
)

if(DXSPLAT_BUILD_VIEWER)
  if(NOT WIN32)
    message(STATUS "directxsplat_viewer skipped: requires WIN32")
  elseif(NOT TARGET imgui)
    message(STATUS "directxsplat_viewer skipped: imgui target not available")
  else()
    add_executable(directxsplat_viewer WIN32
      src/app/main.cpp
      ${DXSPLAT_APP_SOURCES}
    )

    set_target_properties(directxsplat_viewer PROPERTIES OUTPUT_NAME DirectXSplatViewer)

    target_include_directories(directxsplat_viewer
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    if(COMMAND directxsplat_apply_project_warnings)
      directxsplat_apply_project_warnings(directxsplat_viewer)
    endif()

    target_link_libraries(directxsplat_viewer
      PRIVATE
        DirectXSplat::DirectXSplat
        imgui
        d3d12
        dxgi
        dxguid
        d3dcompiler
        dwmapi
        shell32
        ole32
    )

    if(COMMAND directxsplat_stage_dxc_runtime)
      directxsplat_stage_dxc_runtime(directxsplat_viewer)
    endif()
  endif()
endif()

set(directxsplat_install_cmakedir ${CMAKE_INSTALL_LIBDIR}/cmake/DirectXSplat)

install(TARGETS dxsplat
  EXPORT DirectXSplatTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT DirectXSplatTargets
  FILE DirectXSplatTargets.cmake
  NAMESPACE DirectXSplat::
  DESTINATION ${directxsplat_install_cmakedir}
)

configure_package_config_file(
  ${DIRECTXSPLAT_CMAKE_DIR}/DirectXSplatConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/DirectXSplatConfig.cmake
  INSTALL_DESTINATION ${directxsplat_install_cmakedir}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/DirectXSplatConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/DirectXSplatConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/DirectXSplatConfigVersion.cmake
  DESTINATION ${directxsplat_install_cmakedir}
)

if(DIRECTXSPLAT_DXCOMPILER_DLL AND DIRECTXSPLAT_DXIL_DLL)
  install(FILES
    ${DIRECTXSPLAT_DXCOMPILER_DLL}
    ${DIRECTXSPLAT_DXIL_DLL}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()
