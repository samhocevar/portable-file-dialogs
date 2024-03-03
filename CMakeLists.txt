cmake_minimum_required(VERSION 3.1.0)

project(portable_file_dialogs VERSION 1.00 LANGUAGES CXX)

SET(PFD_STATIC_LIB OFF CACHE BOOL "Build portable-file-dialogs as a static library")

if(PFD_STATIC_LIB)
    set(PFD_SRC portable-file-dialogs.cpp)
    add_library(portable-file-dialogs ${PFD_SRC})
else()
    add_library(${PROJECT_NAME} INTERFACE)
    target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
endif()