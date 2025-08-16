cmake_minimum_required(VERSION 3.16)
project(AfterHours)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Set build type specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
    add_definitions(-DAFTER_HOURS_DEBUG)
    add_definitions(-DENABLE_PROFILING)
    message(STATUS "Build type: Debug - Profiling enabled")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DRELEASE)
    message(STATUS "Build type: Release - Profiling disabled")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_definitions(-DRELEASE)
    message(STATUS "Build type: RelWithDebInfo - Profiling disabled")
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    add_definitions(-DRELEASE)
    message(STATUS "Build type: MinSizeRel - Profiling disabled")
endif()

# Source files
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.h"
    "example/*.cpp"
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

# Compiler specific flags
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Link libraries (add any required libraries here)
# target_link_libraries(${PROJECT_NAME} PRIVATE ...)

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)