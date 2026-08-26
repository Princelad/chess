add_library(chessuci STATIC src/engine.cpp)

target_include_directories(chessuci PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(chessuci PUBLIC chesscore)
