add_library(chessnet STATIC
    src/protocol.cpp
)

target_include_directories(chessnet PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(chessnet PUBLIC chesscore ${CHESS_SFML_NETWORK_TARGET})
