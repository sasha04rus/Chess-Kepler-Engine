#include <stdio.h>
#include <string.h>

#include "api/api.h"

static int best_move_callback_count = 0;
static KeplerStatus callback_control_status = KEPLER_STATUS_OK;

static void KEPLER_CALL OnInfo(const KeplerSearchInfo* info, void* user_data) {
    (void)info;
    (void)user_data;
}

static void KEPLER_CALL OnBestMove(const char* move, void* user_data) {
    (void)user_data;
    if (move != NULL && move[0] != '\0')
        ++best_move_callback_count;
    callback_control_status = kepler_stop();
}

static int Expect(KeplerStatus actual, KeplerStatus expected, const char* operation) {
    if (actual == expected)
        return 1;

    fprintf(
        stderr,
        "%s returned %s instead of %s: %s\n",
        operation,
        kepler_status_string(actual),
        kepler_status_string(expected),
        kepler_last_error()
    );
    return 0;
}

int main(void) {
    static const char* start_fen =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    char output[4096];
    uint64_t required_size = 0;
    int32_t is_check = -1;

    if (kepler_api_version() != KEPLER_API_VERSION)
        return 1;

    if (!Expect(kepler_set_info_callback(OnInfo, NULL), KEPLER_STATUS_OK, "set info callback"))
        return 2;
    if (!Expect(kepler_set_best_move_callback(OnBestMove, NULL), KEPLER_STATUS_OK, "set best-move callback"))
        return 3;
    if (!Expect(kepler_init(), KEPLER_STATUS_OK, "init"))
        return 4;
    if (!Expect(kepler_go(10, 1), KEPLER_STATUS_NO_POSITION, "go without position"))
        return 5;
    if (!Expect(
            kepler_get_position(NULL, 0, &required_size),
            KEPLER_STATUS_NO_POSITION,
            "get position without position"
        ))
        return 6;
    if (strlen(kepler_last_error()) == 0)
        return 7;
    if (!Expect(kepler_set_position("invalid"), KEPLER_STATUS_INVALID_ARGUMENT, "invalid FEN"))
        return 8;
    if (!Expect(
            kepler_set_position(start_fen),
            KEPLER_STATUS_OK,
            "set position"
        ))
        return 9;
    if (!Expect(
            kepler_get_position(NULL, 0, &required_size),
            KEPLER_STATUS_OK,
            "query position size"
        ))
        return 10;
    if (required_size != strlen(start_fen) + 1)
        return 11;
    if (!Expect(
            kepler_get_position(output, 1, &required_size),
            KEPLER_STATUS_BUFFER_TOO_SMALL,
            "get position into small buffer"
        ))
        return 12;
    if (!Expect(
            kepler_get_position(output, sizeof(output), &required_size),
            KEPLER_STATUS_OK,
            "get position"
        ))
        return 13;
    if (strcmp(output, start_fen) != 0)
        return 14;
    if (!Expect(
            kepler_get_legal_moves(output, sizeof(output), &required_size),
            KEPLER_STATUS_OK,
            "get legal moves"
        ))
        return 15;
    if (strstr(output, "e2e4") == NULL)
        return 16;
    if (!Expect(kepler_get_result(output, sizeof(output), &required_size), KEPLER_STATUS_OK, "get result"))
        return 17;
    if (strcmp(output, "*") != 0)
        return 18;
    if (!Expect(kepler_is_check(&is_check), KEPLER_STATUS_OK, "is check"))
        return 19;
    if (is_check != 0)
        return 20;
    if (!Expect(kepler_make_move("bad"), KEPLER_STATUS_INVALID_ARGUMENT, "invalid move"))
        return 21;
    if (!Expect(kepler_make_move("e2e4"), KEPLER_STATUS_OK, "make move"))
        return 22;
    if (!Expect(
            kepler_get_position(output, sizeof(output), &required_size),
            KEPLER_STATUS_OK,
            "get position after move"
        ))
        return 23;
    if (strncmp(
            output,
            "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 ",
            strlen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 ")
        ) != 0)
        return 24;
    if (!Expect(kepler_set_threads(1), KEPLER_STATUS_OK, "set threads"))
        return 25;
    if (!Expect(kepler_set_multi_pv(1), KEPLER_STATUS_OK, "set multi-PV"))
        return 26;
    if (!Expect(kepler_go(0, 30), KEPLER_STATUS_OK, "go"))
        return 27;
    if (!Expect(kepler_stop(), KEPLER_STATUS_OK, "stop"))
        return 28;
    if (best_move_callback_count != 1)
        return 29;
    if (callback_control_status != KEPLER_STATUS_CALLBACK_CONTEXT)
        return 30;
#if KEPLER_TAR
    if (!Expect(
            kepler_set_position("r3k3/1P6/8/8/8/8/8/4K3 w - - 0 1"),
            KEPLER_STATUS_OK,
            "set TAR promotion position"
        ))
        return 35;
    if (!Expect(
            kepler_get_legal_moves(output, sizeof(output), &required_size),
            KEPLER_STATUS_OK,
            "get TAR promotion moves"
        ))
        return 36;
    if (strstr(output, "b7a8qd8") == NULL)
        return 37;
    if (!Expect(
            kepler_make_move("b7a8d8q"),
            KEPLER_STATUS_INVALID_ARGUMENT,
            "reject return square before promotion"
        ))
        return 38;
    if (!Expect(kepler_make_move("b7a8qd8"), KEPLER_STATUS_OK, "TAR promotion and return"))
        return 39;
    if (!Expect(
            kepler_get_position(output, sizeof(output), &required_size),
            KEPLER_STATUS_OK,
            "get TAR promotion result"
        ))
        return 40;
    if (strncmp(
            output,
            "Q2rk3/8/8/8/8/8/8/4K3 b - - ",
            strlen("Q2rk3/8/8/8/8/8/8/4K3 b - - ")
        ) != 0)
        return 41;
#endif
    if (!Expect(kepler_new_game(), KEPLER_STATUS_OK, "new game"))
        return 31;
    if (!Expect(kepler_shutdown(), KEPLER_STATUS_OK, "shutdown"))
        return 32;
    if (!Expect(kepler_shutdown(), KEPLER_STATUS_OK, "repeated shutdown"))
        return 33;
    if (!Expect(kepler_stop(), KEPLER_STATUS_NOT_INITIALIZED, "stop after shutdown"))
        return 34;

    return 0;
}
