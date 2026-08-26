add_library(chesscore STATIC src/board.cpp src/fen.cpp src/movegen.cpp src/san.cpp src/uci_move.cpp)

target_include_directories(chesscore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
