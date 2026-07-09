cmake_minimum_required(VERSION 3.16)

project(ChessKepler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_TAR_EVAL "Use tar evaluation/search implementation" OFF)

set(SOURCES
    src/main.cpp
    src/board/board.cpp
    src/hashing/zobrist.cpp
    src/movegen/generate_moves.cpp
    src/movegen/legal_test.cpp
    src/moves/to_string.cpp
    src/search/search.cpp
    src/time/time_manager.cpp
    src/tt/tt.cpp
    src/uci/uci.cpp
)

if(USE_TAR_EVAL)
    list(APPEND SOURCES
        src/eval/tar/evaluate_position.cpp
        src/search/tar/minimax.cpp
    )
else()
    list(APPEND SOURCES
        src/eval/standard/evaluate_position.cpp
        src/search/standard/minimax.cpp
    )
endif()

add_executable(ChessKepler ${SOURCES})

target_include_directories(ChessKepler PRIVATE
    src
    src/board
    src/eval
    src/eval/standard
    src/eval/tar
    src/hashing
    src/movegen
    src/moves
    src/moves/standard
    src/moves/tar
    src/search
    src/search/standard
    src/search/tar
    src/time
    src/tt
    src/uci
    src/utils
)

if (MSVC)
    target_compile_options(ChessKepler PRIVATE /W4)
else()
    target_compile_options(ChessKepler PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(ChessKepler PRIVATE atomic)
endif()