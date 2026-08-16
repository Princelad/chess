add_executable(chess-client src/main.cpp)

target_link_libraries(chess-client PRIVATE chesscore chessnet)
