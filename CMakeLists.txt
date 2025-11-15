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

# ============================================================================
# Profile-Guided Optimization (PGO) Configuration
# ============================================================================

option(ENABLE_PGO "Enable Profile-Guided Optimization (two-pass compilation)" OFF)
option(PGO_GENERATE "Generate PGO profile data" OFF)
option(PGO_USE "Use PGO profile data for final compilation" OFF)

# PGO flags for different compilers
if(ENABLE_PGO AND PGO_GENERATE)
    if(MSVC)
        add_compile_options(/GL)  # Whole program optimization
    else()
        add_compile_options(-fprofile-generate)
        string(APPEND CMAKE_EXE_LINKER_FLAGS " -fprofile-generate")
    endif()
    message(STATUS "PGO: Generating profile data")
elseif(ENABLE_PGO AND PGO_USE)
    if(MSVC)
        add_compile_options(/GL)
    else()
        add_compile_options(-fprofile-use -fprofile-correction)
        string(APPEND CMAKE_EXE_LINKER_FLAGS " -fprofile-use -fprofile-correction")
    endif()
    message(STATUS "PGO: Using profile data")
endif()

# ============================================================================
# Memory Safety Option (Fil-C) - Dual Version Support
# ============================================================================

option(BUILD_SAFE_VERSION "Build memory-safe version with Fil-C compiler" OFF)

# Check if Fil-C is available
find_program(FILC_COMPILER filc)
if(BUILD_SAFE_VERSION AND NOT FILC_COMPILER)
    message(WARNING "BUILD_SAFE_VERSION enabled but 'filc' compiler not found in PATH")
    message(STATUS "Install Fil-C from https://fil-c.org/ to build memory-safe version")
    set(BUILD_SAFE_VERSION OFF)
elseif(BUILD_SAFE_VERSION)
    message(STATUS "Memory safety: Will build both standard and Fil-C memory-safe versions")
else()
    message(STATUS "Memory safety: Disabled (building standard version only)")
    message(STATUS "  Use -DBUILD_SAFE_VERSION=ON to build memory-safe version with Fil-C")
endif()

# Enable SIMD optimizations and aggressive optimization flags
if(MSVC)
    # MSVC optimizations
    add_compile_options(/arch:AVX2)
    add_compile_options(/O2)  # Favor speed
    add_compile_options(/Oi)  # Intrinsic functions
    add_compile_options(/Ot)  # Favor fast code
    add_compile_options(/W4)  # Warning level 4
else()
    # GCC/Clang optimizations
    add_compile_options(-march=native)
    add_compile_options(-O3)  # Maximum optimization
    add_compile_options(-flto)  # Link-time optimization
    add_compile_options(-funroll-loops)  # Unroll loops
    add_compile_options(-finline-functions)  # Inline functions
    add_compile_options(-ffast-math)  # Fast floating-point math
    add_compile_options(-fomit-frame-pointer)  # Omit frame pointer
    add_compile_options(-fno-exceptions)  # No C++ exceptions

    # Pedantic warnings
    add_compile_options(-Wall)      # All warnings
    add_compile_options(-Wextra)    # Extra warnings
    add_compile_options(-Wpedantic) # Pedantic warnings
    add_compile_options(-Wstrict-prototypes)  # Strict prototypes
    add_compile_options(-Wwrite-strings)      # Write-strings
    add_compile_options(-Wshadow)             # Variable shadowing
    add_compile_options(-Wconversion)         # Implicit conversions

    # Disable specific warnings that conflict with intentional patterns
    add_compile_options(-Wno-sign-conversion) # Allow signed/unsigned conversion
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
# Memory-Safe Versions (Fil-C) - Optional
# ============================================================================

if(BUILD_SAFE_VERSION AND FILC_COMPILER)
    message(STATUS "Building memory-safe versions with Fil-C compiler: ${FILC_COMPILER}")

    # Create object files using Fil-C
    set(DASHEM_SAFE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/dashem_safe.o")

    add_custom_command(
        OUTPUT ${DASHEM_SAFE_OBJ}
        COMMAND ${FILC_COMPILER}
            -Wall -Wextra -Wpedantic
            -O2 -c
            -I${CMAKE_CURRENT_SOURCE_DIR}/src
            -o ${DASHEM_SAFE_OBJ}
            ${CMAKE_CURRENT_SOURCE_DIR}/src/dashem.c
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/dashem.c
        COMMENT "Compiling dashem.c with Fil-C for memory safety"
        VERBATIM
    )

    add_custom_target(dashem_safe_obj DEPENDS ${DASHEM_SAFE_OBJ})

    # Memory-safe shared library (using filc-compiled object)
    add_library(dashem_safe SHARED)
    target_sources(dashem_safe PRIVATE ${DASHEM_SAFE_OBJ})
    add_dependencies(dashem_safe dashem_safe_obj)

    target_include_directories(dashem_safe PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
    )

    # Memory-safe static library (using filc-compiled object)
    add_library(dashem_safe_static STATIC)
    target_sources(dashem_safe_static PRIVATE ${DASHEM_SAFE_OBJ})
    add_dependencies(dashem_safe_static dashem_safe_obj)

    target_include_directories(dashem_safe_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
    )

    message(STATUS "Memory-safe versions available: dashem_safe (shared) and dashem_safe_static (static)")
    message(STATUS "These versions compiled with Fil-C for runtime memory safety checks")

endif()

# ============================================================================
# Tests
# ============================================================================

enable_testing()

add_executable(test_dashem tests/test_dashem.c)
target_link_libraries(test_dashem PRIVATE dashem_static)
target_include_directories(test_dashem PRIVATE src)

add_test(NAME DashemTests COMMAND test_dashem)

# Comprehensive correctness tests with edge cases
add_executable(comprehensive_tests tests/comprehensive_tests.c)
target_link_libraries(comprehensive_tests PRIVATE dashem_static)
target_include_directories(comprehensive_tests PRIVATE src)

add_test(NAME ComprehensiveTests COMMAND comprehensive_tests)

# Fuzzing test suite
add_executable(fuzzer tests/fuzzer.c)
target_link_libraries(fuzzer PRIVATE dashem_static)
target_include_directories(fuzzer PRIVATE src)

add_test(NAME FuzzingTests COMMAND fuzzer)

# ============================================================================
# Benchmarks
# ============================================================================

add_executable(bench_dashem benchmarks/bench_dashem.c)
target_link_libraries(bench_dashem PRIVATE dashem_static)
target_include_directories(bench_dashem PRIVATE src)

# ============================================================================
# Installation
# ============================================================================

if(BUILD_SAFE_VERSION AND FILC_COMPILER)
    install(TARGETS dashem dashem_static dashem_safe dashem_safe_static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
else()
    install(TARGETS dashem dashem_static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
endif()

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
