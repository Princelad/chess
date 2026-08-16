if(BUILD_TESTING)
    add_executable(chess-tests test_main.cpp)
    add_test(NAME chess-tests COMMAND chess-tests)
endif()
