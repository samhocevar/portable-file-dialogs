cmake_minimum_required(VERSION 3.1.0)

project(portable_file_dialogs VERSION 1.00 LANGUAGES CXX)

file(GLOB_RECURSE PFD_HEADERS "*.h")
file(GLOB_RECURSE PFD_SOURCES "*.cpp")

add_library(${PROJECT_NAME} STATIC ${PFD_HEADERS} ${PFD_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
