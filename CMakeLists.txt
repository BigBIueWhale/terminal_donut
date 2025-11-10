# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

project(torus LANGUAGES CXX)

# ---- Build type (default to Release if unspecified) --------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# ---- C++ standard: use ISO C++ (no compiler extensions like gnu++17) --------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Target -----------------------------------------------------------------
add_executable(torus main.cpp)

# ---- Warnings ---------------------------------------------------------------
target_compile_options(torus PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

option(WARN_AS_ERROR "Treat warnings as errors" OFF)
if(WARN_AS_ERROR)
  target_compile_options(torus PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Werror>
  )
endif()

# ---- Dependencies -----------------------------------------------------------
# Link against (n)curses; required by the program using <curses.h>
find_package(Curses REQUIRED)
target_link_libraries(torus PRIVATE ${CURSES_LIBRARIES})
target_include_directories(torus PRIVATE ${CURSES_INCLUDE_DIR})

# ---- Optional quality-of-life toggles ---------------------------------------
# LTO/IPO in Release if the toolchain supports it
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
  set_property(TARGET torus PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

# AddressSanitizer (Debug builds; Clang/GCC)
option(ENABLE_ASAN "Enable AddressSanitizer (Debug only)" OFF)
if(ENABLE_ASAN AND CMAKE_BUILD_TYPE MATCHES "Debug")
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(torus PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(torus PRIVATE -fsanitize=address)
  endif()
endif()

# Use ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# ---- Install ----------------------------------------------------------------
install(TARGETS torus RUNTIME DESTINATION bin)
