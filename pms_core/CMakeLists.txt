cmake_minimum_required(VERSION 3.25)

# ProjectionMappingSimulatorが使用するhost-owned D3D12 rendererだけを公開する。
# viewer、window、input、WIC、ZIP、SOG、benchmark、testはproduction targetへ含めない。
set(DIRECTXSPLAT_PMS_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(DIRECTXSPLAT_PMS_SOURCE_ROOT "${DIRECTXSPLAT_PMS_ROOT}/directxsplat")
set(DIRECTXSPLAT_PMS_SHADER_DIR "${DIRECTXSPLAT_PMS_SOURCE_ROOT}/src/shaders")
set(DIRECTXSPLAT_PMS_GPU_SORT_DIR
    "${DIRECTXSPLAT_PMS_ROOT}/3rdparty/GPUSorting/GPUSortingD3D12")
set(DIRECTXSPLAT_PMS_GPU_SORT_SHADER_DIR "${DIRECTXSPLAT_PMS_GPU_SORT_DIR}/Shaders")

set(DIRECTXSPLAT_PMS_EMBEDDED_SHADER_HEADER
    "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedShaders.h")
add_custom_command(
    OUTPUT "${DIRECTXSPLAT_PMS_EMBEDDED_SHADER_HEADER}"
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT=${DIRECTXSPLAT_PMS_EMBEDDED_SHADER_HEADER}
        -DCOMPUTE_INPUT=${DIRECTXSPLAT_PMS_SHADER_DIR}/gaussian_compute.hlsl
        -DRASTER_INPUT=${DIRECTXSPLAT_PMS_SHADER_DIR}/gaussian_raster.hlsl
        -DONESWEEP_INPUT=${DIRECTXSPLAT_PMS_GPU_SORT_SHADER_DIR}/OneSweep.hlsl
        -DSWEEP_COMMON_INPUT=${DIRECTXSPLAT_PMS_GPU_SORT_SHADER_DIR}/SweepCommon.hlsl
        -DSORT_COMMON_INPUT=${DIRECTXSPLAT_PMS_GPU_SORT_SHADER_DIR}/SortCommon.hlsl
        -P "${DIRECTXSPLAT_PMS_ROOT}/cmake/GenerateEmbeddedShaders.cmake"
    DEPENDS
        "${DIRECTXSPLAT_PMS_ROOT}/cmake/GenerateEmbeddedShaders.cmake"
        "${DIRECTXSPLAT_PMS_SHADER_DIR}/gaussian_compute.hlsl"
        "${DIRECTXSPLAT_PMS_SHADER_DIR}/gaussian_raster.hlsl"
        "${DIRECTXSPLAT_PMS_GPU_SORT_SHADER_DIR}/OneSweep.hlsl"
        "${DIRECTXSPLAT_PMS_GPU_SORT_SHADER_DIR}/SweepCommon.hlsl"
        "${DIRECTXSPLAT_PMS_GPU_SORT_SHADER_DIR}/SortCommon.hlsl"
    VERBATIM
)

add_library(directxsplat_pms_core STATIC
    "${DIRECTXSPLAT_PMS_SOURCE_ROOT}/src/core/common/Bounding.cpp"
    "${DIRECTXSPLAT_PMS_SOURCE_ROOT}/src/core/common/Math.cpp"
    "${DIRECTXSPLAT_PMS_SOURCE_ROOT}/src/core/renderer/D3D12Context.cpp"
    "${DIRECTXSPLAT_PMS_SOURCE_ROOT}/src/core/renderer/GpuDataPacking.cpp"
    "${DIRECTXSPLAT_PMS_SOURCE_ROOT}/src/core/renderer/Renderer.cpp"
    "${DIRECTXSPLAT_PMS_SOURCE_ROOT}/src/core/renderer/raster/GaussianRasterPipeline.cpp"
    "${DIRECTXSPLAT_PMS_SOURCE_ROOT}/src/core/renderer/sort/OneSweep.cpp"
    "${DIRECTXSPLAT_PMS_EMBEDDED_SHADER_HEADER}"
)
add_library(DirectXSplat::PmsCore ALIAS directxsplat_pms_core)

target_compile_features(directxsplat_pms_core PUBLIC cxx_std_20)
target_compile_definitions(directxsplat_pms_core
    PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
    PUBLIC
        NOMINMAX
)
target_compile_options(directxsplat_pms_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
target_include_directories(directxsplat_pms_core
    PUBLIC
        "${DIRECTXSPLAT_PMS_SOURCE_ROOT}/include"
    PRIVATE
        "${DIRECTXSPLAT_PMS_SOURCE_ROOT}/src"
        "${DIRECTXSPLAT_PMS_SOURCE_ROOT}/src/core"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_include_directories(directxsplat_pms_core SYSTEM PRIVATE
    "${DIRECTXSPLAT_PMS_ROOT}/3rdparty"
    "${DIRECTXSPLAT_PMS_GPU_SORT_DIR}"
)
target_link_libraries(directxsplat_pms_core
    PUBLIC
        d3d12
        dxgi
        dxguid
        d3dcompiler
        dxcompiler
)

set_target_properties(directxsplat_pms_core PROPERTIES
    FOLDER "ThirdParty/DirectXSplat"
)
