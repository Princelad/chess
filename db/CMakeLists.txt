add_library(chessdb STATIC
    src/database.cpp
    src/sha256.cpp
)

target_include_directories(chessdb PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(chessdb PUBLIC chessnet ${CHESS_SQLITE_TARGET})
