add_library(chessnet INTERFACE)

target_include_directories(chessnet INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(chessnet INTERFACE chesscore ${CHESS_SFML_NETWORK_TARGET})
