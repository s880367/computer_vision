cmake_minimum_required(VERSION 2.8)
project(final)
find_package(OpenCV REQUIRED)
add_executable( final main.cpp)
target_link_libraries(final ${OpenCV_LIBS})
