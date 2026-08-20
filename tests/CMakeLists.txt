if(BUILD_TESTING)
    include(FetchContent)

    set(BUILD_GMOCK OFF CACHE BOOL "Build gmock" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "Install gtest" FORCE)

    set(_gtest_build_testing ${BUILD_TESTING})
    set(BUILD_TESTING OFF CACHE BOOL "Build tests" FORCE)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(googletest)
    set(BUILD_TESTING ${_gtest_build_testing} CACHE BOOL "Build tests" FORCE)

    add_executable(chess-tests
        test_main.cpp
        core/types_test.cpp
        core/board_test.cpp
        core/state_test.cpp
        core/startpos_test.cpp
        core/move_test.cpp
        core/fen_test.cpp
        core/movegen_test.cpp
        core/makemove_test.cpp
        core/san_test.cpp
        core/perft_test.cpp
        core/endgame_test.cpp
        net/protocol_test.cpp
        server/matchmaker_test.cpp
        server/match_test.cpp
        client/connection_test.cpp
        ${CMAKE_SOURCE_DIR}/client/src/connection.cpp)

    target_include_directories(chess-tests PRIVATE
        ${CMAKE_SOURCE_DIR}/client/src)

    target_link_libraries(chess-tests PRIVATE chesscore chessnet chessserver GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(chess-tests)
endif()
