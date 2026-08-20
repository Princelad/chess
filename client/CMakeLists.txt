add_executable(chess-client src/main.cpp src/connection.cpp)

target_link_libraries(chess-client PRIVATE chesscore chessnet
    ${CHESS_SFML_GRAPHICS_TARGET}
    ${CHESS_SFML_WINDOW_TARGET}
    ${CHESS_SFML_NETWORK_TARGET})
