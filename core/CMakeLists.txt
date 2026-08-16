add_library(chesscore STATIC src/board.cpp src/fen.cpp)

target_include_directories(chesscore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
