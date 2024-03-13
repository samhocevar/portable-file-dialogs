add_executable(kill ${CMAKE_CURRENT_SOURCE_DIR}/kill.cpp)
target_link_libraries(kill PRIVATE ${PROJECT_NAME})

add_executable(example ${CMAKE_CURRENT_SOURCE_DIR}/example.cpp)
target_link_libraries(example PRIVATE ${PROJECT_NAME})
