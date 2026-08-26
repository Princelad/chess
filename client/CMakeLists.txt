add_library(chessclient STATIC
    src/connection.cpp
    src/app.cpp
    src/boardview.cpp
    src/screens/connect_screen.cpp
    src/screens/game_screen.cpp
    src/screens/game_over_screen.cpp
    src/screens/local_game_screen.cpp
    src/screens/hud.cpp)
target_include_directories(chessclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(chessclient PUBLIC chessnet chesscore chessuci
    ${CHESS_SFML_GRAPHICS_TARGET}
    ${CHESS_SFML_WINDOW_TARGET}
    ${CHESS_SFML_NETWORK_TARGET})

add_executable(chess-client src/main.cpp)
target_link_libraries(chess-client PRIVATE chessclient
    ${CHESS_SFML_GRAPHICS_TARGET}
    ${CHESS_SFML_WINDOW_TARGET}
    ${CHESS_SFML_NETWORK_TARGET})
