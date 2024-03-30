# For Ubuntu 20.04 Focal
cmake_minimum_required(VERSION 3.16)
project(
  ejobdp
  VERSION 0.1.0
  DESCRIPTION "Job dispatch for multi-threading in enclaves"
  HOMEPAGE_URL "https://github.com/myl7/ejobdp"
  LANGUAGES CXX
)

add_library(ejobdp INTERFACE)
target_include_directories(ejobdp INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_property(TARGET ejobdp PROPERTY CXX_STANDARD 17)

find_package(Doxygen)
if(Doxygen_FOUND)
  doxygen_add_docs(ejobdp_doxygen "${CMAKE_SOURCE_DIR}")
else()
  message(STATUS "Doxygen not found, not building documentation")
endif()
add_custom_target(doc DEPENDS ejobdp_doxygen)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
