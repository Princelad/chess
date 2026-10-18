add_library(chessdb STATIC
    src/auth.cpp
    src/database.cpp
    src/game_store.cpp
    src/glicko2.cpp
    src/player_store.cpp
    src/sha256.cpp
)

target_include_directories(chessdb PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(chessdb PUBLIC chessnet ${CHESS_SQLITE_TARGET})
