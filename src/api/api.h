#ifndef KEPLER_API_H
#define KEPLER_API_H

#include <stdint.h>

#if defined(_WIN32)
    #define KEPLER_CALL __cdecl
    #if defined(KEPLER_API_EXPORTS)
        #define KEPLER_API __declspec(dllexport)
    #else
        #define KEPLER_API __declspec(dllimport)
    #endif
#else
    #define KEPLER_CALL
    #if defined(KEPLER_API_EXPORTS) && (defined(__GNUC__) || defined(__clang__))
        #define KEPLER_API __attribute__((visibility("default")))
    #else
        #define KEPLER_API
    #endif
#endif

#if defined(__cplusplus)
    #define KEPLER_NOEXCEPT noexcept
extern "C" {
#else
    #define KEPLER_NOEXCEPT
#endif

#define KEPLER_API_VERSION 2u

typedef enum KeplerStatus {
    KEPLER_STATUS_OK = 0,
    KEPLER_STATUS_NOT_INITIALIZED = 1,
    KEPLER_STATUS_INVALID_ARGUMENT = 2,
    KEPLER_STATUS_NO_POSITION = 3,
    KEPLER_STATUS_SEARCH_RUNNING = 4,
    KEPLER_STATUS_CALLBACK_CONTEXT = 5,
    KEPLER_STATUS_INTERNAL_ERROR = 6,
    KEPLER_STATUS_BUFFER_TOO_SMALL = 7
} KeplerStatus;

typedef struct KeplerSearchInfo {
    int32_t depth;
    int32_t evaluation;
    int32_t mate_in;
    uint64_t time_ms;
    uint64_t nodes;
    uint64_t nps;

    /* Valid only for the duration of the info callback. */
    const char* pv;
} KeplerSearchInfo;

/*
 * Callbacks run on the engine's search thread. The callback must copy any data
 * it wants to retain. Engine-control functions called from a callback return
 * KEPLER_STATUS_CALLBACK_CONTEXT.
 */
typedef void (KEPLER_CALL *KeplerInfoCallback)(
    const KeplerSearchInfo* info,
    void* user_data
);
typedef void (KEPLER_CALL *KeplerBestMoveCallback)(
    const char* move,
    void* user_data
);

KEPLER_API uint32_t KEPLER_CALL kepler_api_version(void) KEPLER_NOEXCEPT;
KEPLER_API const char* KEPLER_CALL kepler_status_string(KeplerStatus status) KEPLER_NOEXCEPT;

/* Valid until the next status-returning API call on the same calling thread. */
KEPLER_API const char* KEPLER_CALL kepler_last_error(void) KEPLER_NOEXCEPT;

KEPLER_API KeplerStatus KEPLER_CALL kepler_init(void) KEPLER_NOEXCEPT;
KEPLER_API KeplerStatus KEPLER_CALL kepler_shutdown(void) KEPLER_NOEXCEPT;
KEPLER_API KeplerStatus KEPLER_CALL kepler_set_position(const char* fen) KEPLER_NOEXCEPT;

/*
 * String getters write a null-terminated UTF-8 string to buffer.
 * required_size, when non-null, receives the number of bytes required,
 * including the null terminator. Pass buffer = NULL and buffer_size = 0 to
 * query the required size without copying the value.
 *
 * Legal moves use UCI notation and are separated by single spaces.
 */
KEPLER_API KeplerStatus KEPLER_CALL kepler_get_position(
    char* buffer,
    uint64_t buffer_size,
    uint64_t* required_size
) KEPLER_NOEXCEPT;
KEPLER_API KeplerStatus KEPLER_CALL kepler_get_legal_moves(
    char* buffer,
    uint64_t buffer_size,
    uint64_t* required_size
) KEPLER_NOEXCEPT;
KEPLER_API KeplerStatus KEPLER_CALL kepler_get_result(
    char* buffer,
    uint64_t buffer_size,
    uint64_t* required_size
) KEPLER_NOEXCEPT;
KEPLER_API KeplerStatus KEPLER_CALL kepler_is_check(int32_t* is_check) KEPLER_NOEXCEPT;

/* The move must be legal in the current position. */
KEPLER_API KeplerStatus KEPLER_CALL kepler_make_move(const char* move) KEPLER_NOEXCEPT;
KEPLER_API KeplerStatus KEPLER_CALL kepler_set_threads(int32_t threads) KEPLER_NOEXCEPT;
KEPLER_API KeplerStatus KEPLER_CALL kepler_set_multi_pv(int32_t multi_pv) KEPLER_NOEXCEPT;
KEPLER_API KeplerStatus KEPLER_CALL kepler_go(int32_t movetime_ms, int32_t depth) KEPLER_NOEXCEPT;
KEPLER_API KeplerStatus KEPLER_CALL kepler_go_with_clock(
    int32_t white_time_ms,
    int32_t black_time_ms,
    int32_t white_increment_ms,
    int32_t black_increment_ms,
    int32_t depth
) KEPLER_NOEXCEPT;
KEPLER_API KeplerStatus KEPLER_CALL kepler_stop(void) KEPLER_NOEXCEPT;
KEPLER_API KeplerStatus KEPLER_CALL kepler_new_game(void) KEPLER_NOEXCEPT;

/* Passing a null callback unregisters it. Registration is allowed before init. */
KEPLER_API KeplerStatus KEPLER_CALL kepler_set_info_callback(
    KeplerInfoCallback callback,
    void* user_data
) KEPLER_NOEXCEPT;

KEPLER_API KeplerStatus KEPLER_CALL kepler_set_best_move_callback(
    KeplerBestMoveCallback callback,
    void* user_data
) KEPLER_NOEXCEPT;

#if defined(__cplusplus)
}
#endif

#undef KEPLER_NOEXCEPT

#endif
