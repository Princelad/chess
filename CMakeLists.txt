cmake_minimum_required(VERSION 3.16)

project(chess CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" ON)
include(CTest)

find_package(SFML 3 COMPONENTS Graphics Window Network Audio QUIET)
if(SFML_FOUND)
    set(CHESS_SFML_GRAPHICS_TARGET SFML::Graphics)
    set(CHESS_SFML_WINDOW_TARGET SFML::Window)
    set(CHESS_SFML_NETWORK_TARGET SFML::Network)
    set(CHESS_SFML_AUDIO_TARGET SFML::Audio)
else()
    message(STATUS "SFML 3 not found - building SFML 3.1.0 from source")
    include(FetchContent)
    set(SFML_BUILD_AUDIO ON CACHE BOOL "Build SFML audio module" FORCE)
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
    set(CHESS_SFML_AUDIO_TARGET sfml-audio)
endif()

# Phase 10.1: persistence uses the system SQLite library (no vendored copy).
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    message(FATAL_ERROR
        "SQLite3 (system libsqlite3) not found — install the development "
        "package (e.g. libsqlite3-dev on Debian/Ubuntu)")
endif()
set(CHESS_SQLITE_TARGET SQLite::SQLite3)

# Portability note (13.3): uci/ spawns engines through the POSIX process API
# (fork/exec/dup2/waitpid — see README "Platform support"), and server bots
# as well as the GUI's local-play screen link it; all these targets build
# unconditionally, so a Windows build is documented as not yet supported
# rather than half-configurable.
add_subdirectory(core)
add_subdirectory(uci)
add_subdirectory(net)
add_subdirectory(db)
add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(tests)

# Phase 13.2: install targets. Binaries to bindir; the whole assets tree
# (fonts, piece sets, puzzles.pgn) to a share/chess datadir — clients find
# it by exporting CHESS_ASSETS=<prefix>/share/chess/assets (see assetRoots()
# in client/src/asset_paths.h), since the checkout-relative roots a repo
# build relies on don't exist for installed trees.
include(GNUInstallDirs)
install(TARGETS chess-client chess-server
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/chess/assets")
