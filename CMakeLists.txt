cmake_minimum_required(VERSION 3.16)

project(ChessKepler LANGUAGES C CXX)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(
    TAKE_AND_RETURN_VARIANT
    "Use TAR evaluation/search implementation"
    OFF
)

set(KEPLER_CORE_SOURCES
    src/engine/engine.cpp
    src/board/board.cpp
    src/hashing/zobrist.cpp
    src/movegen/generate_moves.cpp
    src/movegen/legal_test.cpp
    src/moves/to_string.cpp
    src/search/search.cpp
    src/time/time_manager.cpp
    src/history/history.cpp
    src/tt/tt.cpp
)

if(TAKE_AND_RETURN_VARIANT)
    list(APPEND KEPLER_CORE_SOURCES
        src/eval/tar/evaluate_position.cpp
        src/search/tar/minimax.cpp
        src/moves/tar/move.cpp
    )
else()
    list(APPEND KEPLER_CORE_SOURCES
        src/eval/standard/evaluate_position.cpp
        src/search/standard/minimax.cpp
    )
endif()

add_library(KeplerCore STATIC ${KEPLER_CORE_SOURCES})

set_target_properties(
    KeplerCore PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

target_compile_definitions(
    KeplerCore
    PUBLIC
    KEPLER_TAR=$<BOOL:${TAKE_AND_RETURN_VARIANT}>
)

target_include_directories(KeplerCore PUBLIC src)

target_link_libraries(KeplerCore PRIVATE Threads::Threads)

add_library(KeplerApi SHARED
    src/api/api.cpp
)

target_compile_definitions(KeplerApi PRIVATE KEPLER_API_EXPORTS)
target_include_directories(KeplerApi PUBLIC src)
target_link_libraries(KeplerApi PRIVATE KeplerCore)

set_target_properties(
    KeplerApi PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

if((CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang") AND NOT APPLE)
    target_link_options(KeplerApi PRIVATE "LINKER:--exclude-libs,ALL")
endif()

add_executable(ChessKepler
    src/main.cpp
    src/uci/uci.cpp
)

target_link_libraries(ChessKepler PRIVATE KeplerCore)

if(ANDROID)
    target_link_options(
        ChessKepler PRIVATE
        "-Wl,-z,max-page-size=16384"
        "-Wl,-z,common-page-size=16384"
    )
endif()

if (MSVC)
    target_compile_options(KeplerCore PRIVATE /W4)
    target_compile_options(KeplerApi PRIVATE /W4)
else()
    target_compile_options(KeplerCore PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(KeplerApi PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(KeplerCore PRIVATE atomic)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(KeplerApiCTest
        tests/api_test.c
    )
    target_compile_definitions(
        KeplerApiCTest PRIVATE
        KEPLER_TAR=$<BOOL:${TAKE_AND_RETURN_VARIANT}>
    )
    target_link_libraries(KeplerApiCTest PRIVATE KeplerApi)
    add_test(NAME api_c COMMAND KeplerApiCTest)

    add_executable(ChessKeplerPerft
        tests/perft_test.cpp
        src/board/board.cpp
        src/hashing/zobrist.cpp
        src/movegen/generate_moves.cpp
        src/movegen/legal_test.cpp
        src/moves/to_string.cpp
    )

    target_include_directories(ChessKeplerPerft PRIVATE
        src
        src/board
        src/hashing
        src/movegen
        src/moves
        src/moves/standard
        src/moves/tar
        src/utils
        src/eval
        src/eval/standard
    )

    if(MSVC)
        target_compile_options(
            ChessKeplerPerft PRIVATE /W4
        )
    else()
        target_compile_options(
            ChessKeplerPerft PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()

    add_test(
        NAME perft
        COMMAND ChessKeplerPerft
    )
    add_executable(ChessKeplerBench
        tests/bench_test.cpp

        src/board/board.cpp
        src/hashing/zobrist.cpp

        src/movegen/generate_moves.cpp
        src/movegen/legal_test.cpp

        src/moves/to_string.cpp

        src/tt/tt.cpp
        src/history/history.cpp

        src/eval/standard/evaluate_position.cpp
        src/search/standard/minimax.cpp
    )

    target_include_directories(ChessKeplerBench PRIVATE
        src
        src/board
        src/eval
        src/eval/standard
        src/hashing
        src/movegen
        src/moves
        src/moves/standard
        src/search
        src/search/standard
        src/tt
        src/history
        src/utils
    )

    if(MSVC)
        target_compile_options(
            ChessKeplerBench PRIVATE /W4
        )
    else()
        target_compile_options(
            ChessKeplerBench PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )

        target_link_libraries(
            ChessKeplerBench PRIVATE atomic
        )
    endif()

    if(TAKE_AND_RETURN_VARIANT)
        set(TAR_TEST_BOARD_SOURCES
            src/board/board.cpp
            src/hashing/zobrist.cpp
            src/movegen/generate_moves.cpp
            src/movegen/legal_test.cpp
            src/moves/to_string.cpp
            src/moves/tar/move.cpp
        )

        add_executable(ChessKeplerTarSearch
            tests/tar_search_test.cpp
            ${TAR_TEST_BOARD_SOURCES}
            src/tt/tt.cpp
            src/history/history.cpp
            src/eval/tar/evaluate_position.cpp
        )
        add_executable(ChessKeplerTarRules
            tests/tar_rules_test.cpp
            ${TAR_TEST_BOARD_SOURCES}
        )
        foreach(tar_test ChessKeplerTarSearch ChessKeplerTarRules)
            target_compile_definitions(${tar_test} PRIVATE KEPLER_TAR=1)
            target_include_directories(${tar_test} PRIVATE src)
            if(MSVC)
                target_compile_options(${tar_test} PRIVATE /W4)
            else()
                target_compile_options(${tar_test} PRIVATE -Wall -Wextra -Wpedantic)
                target_link_libraries(${tar_test} PRIVATE atomic)
            endif()
        endforeach()
        add_test(NAME tar_search COMMAND ChessKeplerTarSearch)
        add_test(NAME tar_rules COMMAND ChessKeplerTarRules)
    endif()
endif()
