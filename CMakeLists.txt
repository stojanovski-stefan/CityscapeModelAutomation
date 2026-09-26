cmake_minimum_required(VERSION 3.20)
project(CityscapeAutomation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# C++ Requests (cpr). Downloaded and built at configure time; it pulls in its
# own libcurl, so no system dependency is required.
include(FetchContent)

# cpr builds curl 8.13+, which wants libpsl (and thus meson) unless told
# otherwise. libpsl only affects cookie-domain validation, which this tool
# does not use, so skip it and drop the extra build dependency.
set(CPR_CURL_USE_LIBPSL OFF)

FetchContent_Declare(cpr
  GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG 1.13.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(cpr)

# CONFIGURE_DEPENDS re-runs the glob on every build, so adding or removing a
# source file does not require a manual re-configure.
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(CityscapeAutomation ${SOURCES})
target_include_directories(CityscapeAutomation PRIVATE src)
target_link_libraries(CityscapeAutomation PRIVATE cpr::cpr)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(CityscapeAutomation PRIVATE -Wall -Wextra)
endif()
