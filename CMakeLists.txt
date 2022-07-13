cmake_minimum_required(VERSION 3.1.0)

project(portable_file_dialogs VERSION 1.00 LANGUAGES CXX)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)

install(TARGETS ${PROJECT_NAME}
    DESTINATION include/${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-config
)

install(EXPORT ${PROJECT_NAME}-config
    DESTINATION lib/cmake/${PROJECT_NAME}
)

install(FILES portable-file-dialogs.h
    DESTINATION include/${PROJECT_NAME}
)
