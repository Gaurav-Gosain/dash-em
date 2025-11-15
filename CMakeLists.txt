cmake_minimum_required(VERSION 3.12)
project(dash-em
    VERSION 1.0.0
    DESCRIPTION "Enterprise-Grade Em-Dash Removal Library"
    LANGUAGES C
)

# ============================================================================
# Build Configuration
# ============================================================================

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Enable SIMD optimizations and aggressive optimization flags
if(MSVC)
    # MSVC optimizations
    add_compile_options(/arch:AVX2)
    add_compile_options(/O2)  # Favor speed
    add_compile_options(/Oi)  # Intrinsic functions
    add_compile_options(/Ot)  # Favor fast code
else()
    # GCC/Clang optimizations
    add_compile_options(-march=native)
    add_compile_options(-O3)  # Maximum optimization
    add_compile_options(-flto)  # Link-time optimization
    add_compile_options(-funroll-loops)  # Unroll loops
    add_compile_options(-finline-functions)  # Inline functions
    add_compile_options(-ffast-math)  # Fast floating-point math
    add_compile_options(-fomit-frame-pointer)  # Omit frame pointer

    # Add -fno-exceptions and -fno-rtti if using C only (not C++)
    add_compile_options(-fno-exceptions)

    # Profile-guided optimization flags (can be enabled with -fprofile-use)
    # add_compile_options(-fprofile-generate)
endif()

# Link-time optimization for GCC/Clang
if(NOT MSVC)
    set(CMAKE_AR "gcc-ar" CACHE FILEPATH "ar program")
    set(CMAKE_RANLIB "gcc-ranlib" CACHE FILEPATH "ranlib program")
    string(APPEND CMAKE_C_FLAGS " -flto")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -flto")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -flto")
endif()

# ============================================================================
# Core Library
# ============================================================================

add_library(dashem SHARED
    src/dashem.c
)

add_library(dashem_static STATIC
    src/dashem.c
)

target_include_directories(dashem PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(dashem_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# Enable all warnings
if(MSVC)
    target_compile_options(dashem PRIVATE /W4)
    target_compile_options(dashem_static PRIVATE /W4)
else()
    target_compile_options(dashem PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(dashem_static PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ============================================================================
# Tests
# ============================================================================

enable_testing()

add_executable(test_dashem tests/test_dashem.c)
target_link_libraries(test_dashem PRIVATE dashem_static)
target_include_directories(test_dashem PRIVATE src)

add_test(NAME DashemTests COMMAND test_dashem)

# ============================================================================
# Benchmarks
# ============================================================================

add_executable(bench_dashem benchmarks/bench_dashem.c)
target_link_libraries(bench_dashem PRIVATE dashem_static)
target_include_directories(bench_dashem PRIVATE src)

# ============================================================================
# Installation
# ============================================================================

install(TARGETS dashem dashem_static
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES src/dashem.h DESTINATION include)

# ============================================================================
# WebAssembly Build
# ============================================================================

if(EMSCRIPTEN OR WASI)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")

    if(EMSCRIPTEN)
        add_library(dashem_wasm src/dashem.c)
        set_target_properties(dashem_wasm PROPERTIES SUFFIX ".js")

        target_compile_options(dashem_wasm PRIVATE
            "SHELL:-s WASM=1"
            "SHELL:-s SIDE_MODULE=1"
        )
    endif()
endif()

# ============================================================================
# Package Configuration
# ============================================================================

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/DashmConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/DashmConfig.cmake
    INSTALL_DESTINATION lib/cmake/dashem
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/DashmConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/DashmConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/DashmConfigVersion.cmake
    DESTINATION lib/cmake/dashem
)
