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
        core/chess960_test.cpp
        core/perft_test.cpp
        core/endgame_test.cpp
        core/uci_move_test.cpp
        core/pgn_test.cpp
        db/sha256_test.cpp
        db/database_test.cpp
        db/auth_test.cpp
        db/glicko2_test.cpp
        db/player_store_test.cpp
        db/rating_class_test.cpp
        db/game_store_test.cpp
        net/protocol_test.cpp
        server/clock_test.cpp
        server/matchmaker_test.cpp
        server/match_test.cpp
        server/bot_match_test.cpp
        server/tournament_test.cpp
        client/connection_test.cpp
        client/move_navigator_test.cpp
        client/board_interaction_test.cpp
        client/config_test.cpp
        client/chat_test.cpp
        client/keyboard_test.cpp
        client/move_animator_test.cpp
        client/premove_test.cpp
        client/eco_test.cpp
        client/sfx_test.cpp
        client/widget_test.cpp
        client/archive_pgn_test.cpp
        client/game_over_transition_test.cpp
        uci/engine_test.cpp
        e2e/e2e_test.cpp
        e2e/rated_test.cpp
        e2e/reconnect_test.cpp
        e2e/takeback_test.cpp
        e2e/rematch_test.cpp
        e2e/chess960_test.cpp
        e2e/arena_test.cpp
        client/puzzle_test.cpp)

    target_link_libraries(chess-tests PRIVATE chesscore chessnet chessclient chessserver chessuci chessdb GTest::gtest_main)
    target_compile_definitions(chess-tests PRIVATE
        CHESS_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")

    include(GoogleTest)
    gtest_discover_tests(chess-tests PROPERTIES
        ENVIRONMENT "MOCK_ENGINE_PATH=${CMAKE_CURRENT_SOURCE_DIR}/uci/mock_engine.py")
endif()
