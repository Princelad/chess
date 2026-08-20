add_library(chessclient STATIC src/connection.cpp)
target_include_directories(chessclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(chessclient PUBLIC chessnet ${CHESS_SFML_NETWORK_TARGET})

add_executable(chess-client src/main.cpp)
target_link_libraries(chess-client PRIVATE chessclient chesscore
    ${CHESS_SFML_GRAPHICS_TARGET}
    ${CHESS_SFML_WINDOW_TARGET}
    ${CHESS_SFML_NETWORK_TARGET})
