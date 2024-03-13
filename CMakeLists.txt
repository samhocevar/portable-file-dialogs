cmake_minimum_required(VERSION 3.10)

project(portable_file_dialogs VERSION 1.00 LANGUAGES CXX)

option(BUILD_EXAMPLES "Build examples" FALSE)

if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
