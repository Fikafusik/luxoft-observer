cmake_minimum_required(VERSION 3.5)

project(Observer)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Observer main.cpp)

target_link_libraries(Observer pthread)

add_subdirectory(plog)

target_link_libraries(Observer plog)