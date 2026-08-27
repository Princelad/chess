add_library(chessserver STATIC
    src/matchmaker.cpp
    src/match.cpp
    src/server.cpp
    src/log.cpp)

target_include_directories(chessserver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(chessserver PUBLIC chesscore chessnet chessuci ${CHESS_SFML_NETWORK_TARGET})

add_executable(chess-server src/main.cpp)
target_link_libraries(chess-server PRIVATE chessserver ${CHESS_SFML_NETWORK_TARGET})
