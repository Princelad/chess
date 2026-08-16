add_executable(chess-server src/main.cpp)

target_link_libraries(chess-server PRIVATE chesscore chessnet ${CHESS_SFML_NETWORK_TARGET})
