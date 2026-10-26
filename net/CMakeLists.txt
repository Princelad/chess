add_library(chessnet STATIC
    src/protocol.cpp
)

target_include_directories(chessnet PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# SFML note (13.2): the FetchContent fallback for systems without SFML 3
# lives in the TOP-LEVEL CMakeLists — chessnet only links whatever
# CHESS_SFML_NETWORK_TARGET the root picked. Keep it that way: a second
# fallback here would double-configure or double-fetch SFML.

target_link_libraries(chessnet PUBLIC chesscore ${CHESS_SFML_NETWORK_TARGET})
