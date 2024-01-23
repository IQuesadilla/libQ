
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  message(STATUS "Checking for VCPKG installation from libQ")
  include(vcpkg_install/vcpkg_install.cmake)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
  project(Q)
endif()

cmake_minimum_required(VERSION 3.10)
#project(Q)

#add_custom_target(vcpkg_install
#    COMMAND ./vcpkg_install.sh
#    COMMENT "Running vcpkg install"
#)

# Set the C++ standard and compilation flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}") # -Wall

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(glm CONFIG REQUIRED)
#find_package(glad CONFIG REQUIRED)

include_directories(.)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/${VCPKG_TARGET_TRIPLET}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/${VCPKG_TARGET_TRIPLET}/include/glm)
include_directories(include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/${VCPKG_TARGET_TRIPLET}/include/glad)

add_library(Q SHARED
    src/shader.cpp
    src/camera.cpp
    src/xml.cpp
    src/log.cpp
)

#add_dependencies(Q vcpkg_install)

# Add executables and link libraries
target_link_libraries(Q PRIVATE
  glm::glm
  #glad::glad
)

set_target_properties(Q PROPERTIES
  VERSION 1.0
  SOVERSION 1
  OUTPUTNAME Q
)
