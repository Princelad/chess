cmake_minimum_required(VERSION 3.16)

project(chess CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" ON)
include(CTest)

find_package(SFML 3 COMPONENTS graphics window network QUIET)
if(SFML_FOUND)
    set(CHESS_SFML_GRAPHICS_TARGET SFML::Graphics)
    set(CHESS_SFML_WINDOW_TARGET SFML::Window)
    set(CHESS_SFML_NETWORK_TARGET SFML::Network)
else()
    message(STATUS "SFML 3 not found - building SFML 3.1.0 from source")
    include(FetchContent)
    set(SFML_BUILD_AUDIO OFF CACHE BOOL "Build SFML audio module" FORCE)
    set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "Build SFML examples" FORCE)
    set(SFML_BUILD_DOC OFF CACHE BOOL "Build SFML documentation" FORCE)
    set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "Build SFML test suite" FORCE)
    set(SFML_USE_SYSTEM_DEPS OFF CACHE BOOL "Use system dependencies" FORCE)
    set(SFML_INSTALL_PKGCONFIG_FILES OFF CACHE BOOL "Install SFML pkg-config files" FORCE)
    FetchContent_Declare(sfml
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        3.1.0
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(sfml)
    set(CHESS_SFML_GRAPHICS_TARGET sfml-graphics)
    set(CHESS_SFML_WINDOW_TARGET sfml-window)
    set(CHESS_SFML_NETWORK_TARGET sfml-network)
endif()

add_subdirectory(core)
add_subdirectory(net)
add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(tests)
