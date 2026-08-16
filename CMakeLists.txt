cmake_minimum_required(VERSION 3.16)

project(chess CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" ON)
include(CTest)

add_subdirectory(core)
add_subdirectory(net)
add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(tests)
