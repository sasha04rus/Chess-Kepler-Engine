#include "api.h"

#include <array>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "engine/engine.h"
#include "hashing/zobrist.h"
#include "search/search.h"
#include "time/time_manager.h"

namespace {

struct InfoCallbackSlot {
    KeplerInfoCallback callback = nullptr;
    void* user_data = nullptr;
};

struct BestMoveCallbackSlot {
    KeplerBestMoveCallback callback = nullptr;
    void* user_data = nullptr;
};

std::unique_ptr<Engine> engine;
std::mutex engine_mutex;
std::mutex callback_mutex;
InfoCallbackSlot info_callback;
BestMoveCallbackSlot best_move_callback;
thread_local std::string last_error;
thread_local bool inside_callback = false;

KeplerStatus Succeed() {
    last_error.clear();
    return KEPLER_STATUS_OK;
}

KeplerStatus Fail(KeplerStatus status, const char* message) {
    try {
        last_error = message;
    } catch (...) {
        last_error.clear();
    }
    return status;
}

KeplerStatus FailFromException(const std::exception& exception) {
    try {
        last_error = exception.what();
    } catch (...) {
        last_error.clear();
    }
    return KEPLER_STATUS_INTERNAL_ERROR;
}

KeplerStatus CheckControlCall() {
    if (inside_callback) {
        return Fail(
            KEPLER_STATUS_CALLBACK_CONTEXT,
            "engine-control functions cannot be called from a search callback"
        );
    }
    return KEPLER_STATUS_OK;
}

KeplerStatus WriteString(
    const std::string& value,
    char* buffer,
    uint64_t buffer_size,
    uint64_t* required_size
) {
    const uint64_t size = static_cast<uint64_t>(value.size()) + 1;
    if (required_size)
        *required_size = size;

    if (!buffer) {
        if (buffer_size != 0)
            return Fail(
                KEPLER_STATUS_INVALID_ARGUMENT,
                "buffer must not be null when buffer_size is non-zero"
            );
        if (!required_size)
            return Fail(
                KEPLER_STATUS_INVALID_ARGUMENT,
                "required_size must not be null when querying output size"
            );
        return Succeed();
    }

    if (buffer_size < size)
        return Fail(KEPLER_STATUS_BUFFER_TOO_SMALL, "output buffer is too small");

    std::memcpy(buffer, value.c_str(), static_cast<std::size_t>(size));
    return Succeed();
}

std::string JoinMoves(const std::vector<std::string>& moves) {
    std::string result;
    for (std::size_t index = 0; index < moves.size(); ++index) {
        if (index != 0)
            result += ' ';
        result += moves[index];
    }
    return result;
}

bool IsSquare(std::string_view square) {
    return square.size() == 2
        && square[0] >= 'a' && square[0] <= 'h'
        && square[1] >= '1' && square[1] <= '8';
}

bool IsPromotion(char piece) {
    return piece == 'n' || piece == 'b' || piece == 'r' || piece == 'q';
}

bool IsMoveSyntaxValid(const char* move) {
    const std::string_view text(move, std::strlen(move));
    if (text.size() < 4)
        return false;
    if (!IsSquare(text.substr(0, 2)) || !IsSquare(text.substr(2, 2)))
        return false;

#if KEPLER_TAR
    if (text.size() == 4)
        return true;
    if (text.size() == 5)
        return IsPromotion(text[4]);
    if (text.size() == 6)
        return IsSquare(text.substr(4, 2));
    return text.size() == 7 && IsPromotion(text[4]) && IsSquare(text.substr(5, 2));
#else
    return text.size() == 4 || (text.size() == 5 && IsPromotion(text[4]));
#endif
}

bool IsUnsignedDecimal(std::string_view text, bool allow_zero) {
    if (text.empty())
        return false;
    for (char character : text)
        if (character < '0' || character > '9')
            return false;
    if (!allow_zero) {
        for (char character : text)
            if (character != '0')
                return true;
        return false;
    }
    return true;
}

bool IsFenValid(const char* fen) {
    const std::string_view text(fen, std::strlen(fen));
    std::array<std::string_view, 6> fields;
    std::size_t field_count = 0;
    std::size_t position = 0;

    while (position < text.size()) {
        while (position < text.size() && text[position] == ' ')
            position++;
        if (position == text.size())
            break;
        if (field_count == fields.size())
            return false;

        const std::size_t begin = position;
        while (position < text.size() && text[position] != ' ')
            position++;
        fields[field_count++] = text.substr(begin, position - begin);
    }

    if (field_count < 4 || field_count > 6)
        return false;

    int rank_count = 1;
    int file_count = 0;
    int white_kings = 0;
    int black_kings = 0;
    for (char character : fields[0]) {
        if (character == '/') {
            if (file_count != 8 || rank_count == 8)
                return false;
            rank_count++;
            file_count = 0;
        } else if (character >= '1' && character <= '8') {
            file_count += character - '0';
        } else if (std::string_view("prnbqkPRNBQK").find(character) != std::string_view::npos) {
            file_count++;
            white_kings += character == 'K';
            black_kings += character == 'k';
        } else {
            return false;
        }
        if (file_count > 8)
            return false;
    }
    if (rank_count != 8 || file_count != 8 || white_kings != 1 || black_kings != 1)
        return false;

    if (fields[1] != "w" && fields[1] != "b")
        return false;

    if (fields[2] != "-") {
        bool seen[4] = {false, false, false, false};
        for (char character : fields[2]) {
            const std::size_t index = std::string_view("KQkq").find(character);
            if (index == std::string_view::npos || seen[index])
                return false;
            seen[index] = true;
        }
    }

    if (fields[3] != "-") {
        if (!IsSquare(fields[3]) || (fields[3][1] != '3' && fields[3][1] != '6'))
            return false;
    }

    if (field_count >= 5 && !IsUnsignedDecimal(fields[4], true))
        return false;
    if (field_count == 6 && !IsUnsignedDecimal(fields[5], false))
        return false;
    return true;
}

void DispatchInfo(const SearchInfo& info) noexcept {
    InfoCallbackSlot slot;
    {
        std::lock_guard<std::mutex> lock(callback_mutex);
        slot = info_callback;
    }
    if (!slot.callback)
        return;

    try {
        std::string pv;
        for (std::size_t i = 0; i < info.pv.size(); ++i) {
            if (i != 0)
                pv += ' ';
            pv += info.pv[i];
        }

        const KeplerSearchInfo api_info{
            static_cast<int32_t>(info.depth),
            static_cast<int32_t>(info.evaluation),
            static_cast<int32_t>(info.mate_in),
            info.time,
            info.nodes,
            info.nps,
            pv.c_str()
        };

        inside_callback = true;
        try {
            slot.callback(&api_info, slot.user_data);
        } catch (...) {
            // Exceptions must never escape through the C ABI or the search thread.
        }
        inside_callback = false;
    } catch (...) {
        inside_callback = false;
    }
}

void DispatchBestMove(const std::string& move) noexcept {
    BestMoveCallbackSlot slot;
    {
        std::lock_guard<std::mutex> lock(callback_mutex);
        slot = best_move_callback;
    }
    if (!slot.callback)
        return;

    inside_callback = true;
    try {
        slot.callback(move.c_str(), slot.user_data);
    } catch (...) {
        // Exceptions must never escape through the C ABI or the search thread.
    }
    inside_callback = false;
}

void ClearCallbacks() {
    std::lock_guard<std::mutex> lock(callback_mutex);
    info_callback = {};
    best_move_callback = {};
}

} // namespace

extern "C" {

uint32_t KEPLER_CALL kepler_api_version(void) noexcept {
    return KEPLER_API_VERSION;
}

const char* KEPLER_CALL kepler_status_string(KeplerStatus status) noexcept {
    switch (status) {
        case KEPLER_STATUS_OK:
            return "ok";
        case KEPLER_STATUS_NOT_INITIALIZED:
            return "engine is not initialized";
        case KEPLER_STATUS_INVALID_ARGUMENT:
            return "invalid argument";
        case KEPLER_STATUS_NO_POSITION:
            return "no position is set";
        case KEPLER_STATUS_SEARCH_RUNNING:
            return "search is already running";
        case KEPLER_STATUS_CALLBACK_CONTEXT:
            return "operation is not allowed from a search callback";
        case KEPLER_STATUS_INTERNAL_ERROR:
            return "internal error";
        case KEPLER_STATUS_BUFFER_TOO_SMALL:
            return "output buffer is too small";
    }
    return "unknown status";
}

const char* KEPLER_CALL kepler_last_error(void) noexcept {
    return last_error.c_str();
}

KeplerStatus KEPLER_CALL kepler_init(void) noexcept {
    if (CheckControlCall() != KEPLER_STATUS_OK)
        return KEPLER_STATUS_CALLBACK_CONTEXT;

    try {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (engine)
            return Succeed();

        zobrist::Init();
        auto new_engine = std::make_unique<Engine>();
        new_engine->SetInfoCallback(DispatchInfo);
        new_engine->SetBestMoveCallback(DispatchBestMove);
        engine = std::move(new_engine);
        return Succeed();
    } catch (const std::exception& exception) {
        return FailFromException(exception);
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unknown exception during initialization");
    }
}

KeplerStatus KEPLER_CALL kepler_shutdown(void) noexcept {
    if (CheckControlCall() != KEPLER_STATUS_OK)
        return KEPLER_STATUS_CALLBACK_CONTEXT;

    try {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (engine) {
            engine->Stop();
            engine.reset();
        }
        ClearCallbacks();
        return Succeed();
    } catch (const std::exception& exception) {
        return FailFromException(exception);
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unknown exception during shutdown");
    }
}

KeplerStatus KEPLER_CALL kepler_set_position(const char* fen) noexcept {
    if (!fen || !IsFenValid(fen))
        return Fail(KEPLER_STATUS_INVALID_ARGUMENT, "fen is null or malformed");
    if (CheckControlCall() != KEPLER_STATUS_OK)
        return KEPLER_STATUS_CALLBACK_CONTEXT;

    try {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (!engine)
            return Fail(KEPLER_STATUS_NOT_INITIALIZED, "call kepler_init first");
        engine->SetPosition(fen);
        return Succeed();
    } catch (const std::exception& exception) {
        return FailFromException(exception);
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unknown exception while setting position");
    }
}

KeplerStatus KEPLER_CALL kepler_get_position(
    char* buffer,
    uint64_t buffer_size,
    uint64_t* required_size
) noexcept {
    try {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (!engine)
            return Fail(KEPLER_STATUS_NOT_INITIALIZED, "call kepler_init first");
        if (engine->IsEmptyPosition())
            return Fail(KEPLER_STATUS_NO_POSITION, "call kepler_set_position first");
        return WriteString(engine->GetPosition(), buffer, buffer_size, required_size);
    } catch (const std::exception& exception) {
        return FailFromException(exception);
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unknown exception while getting position");
    }
}

KeplerStatus KEPLER_CALL kepler_get_legal_moves(
    char* buffer,
    uint64_t buffer_size,
    uint64_t* required_size
) noexcept {
    try {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (!engine)
            return Fail(KEPLER_STATUS_NOT_INITIALIZED, "call kepler_init first");
        if (engine->IsEmptyPosition())
            return Fail(KEPLER_STATUS_NO_POSITION, "call kepler_set_position first");
        return WriteString(
            JoinMoves(engine->GetLegalMoves()),
            buffer,
            buffer_size,
            required_size
        );
    } catch (const std::exception& exception) {
        return FailFromException(exception);
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unknown exception while getting legal moves");
    }
}

KeplerStatus KEPLER_CALL kepler_get_result(
    char* buffer,
    uint64_t buffer_size,
    uint64_t* required_size
) noexcept {
    try {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (!engine)
            return Fail(KEPLER_STATUS_NOT_INITIALIZED, "call kepler_init first");
        if (engine->IsEmptyPosition())
            return Fail(KEPLER_STATUS_NO_POSITION, "call kepler_set_position first");
        return WriteString(engine->GetResult(), buffer, buffer_size, required_size);
    } catch (const std::exception& exception) {
        return FailFromException(exception);
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unknown exception while getting result");
    }
}

KeplerStatus KEPLER_CALL kepler_is_check(int32_t* is_check) noexcept {
    if (!is_check)
        return Fail(KEPLER_STATUS_INVALID_ARGUMENT, "is_check must not be null");

    try {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (!engine)
            return Fail(KEPLER_STATUS_NOT_INITIALIZED, "call kepler_init first");
        if (engine->IsEmptyPosition())
            return Fail(KEPLER_STATUS_NO_POSITION, "call kepler_set_position first");
        *is_check = engine->IsCheck() ? 1 : 0;
        return Succeed();
    } catch (const std::exception& exception) {
        return FailFromException(exception);
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unknown exception while checking position");
    }
}

KeplerStatus KEPLER_CALL kepler_make_move(const char* move) noexcept {
    if (!move || !IsMoveSyntaxValid(move))
        return Fail(KEPLER_STATUS_INVALID_ARGUMENT, "move has invalid syntax");
    if (CheckControlCall() != KEPLER_STATUS_OK)
        return KEPLER_STATUS_CALLBACK_CONTEXT;

    try {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (!engine)
            return Fail(KEPLER_STATUS_NOT_INITIALIZED, "call kepler_init first");
        if (engine->IsEmptyPosition())
            return Fail(KEPLER_STATUS_NO_POSITION, "call kepler_set_position first");
        if (searching.load(std::memory_order_acquire))
            return Fail(KEPLER_STATUS_SEARCH_RUNNING, "stop the search before making a move");
        engine->MakeMove(move);
        return Succeed();
    } catch (const std::exception& exception) {
        return FailFromException(exception);
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unknown exception while making a move");
    }
}

KeplerStatus KEPLER_CALL kepler_set_threads(int32_t threads) noexcept {
    if (threads < 1 || threads > 64)
        return Fail(KEPLER_STATUS_INVALID_ARGUMENT, "threads must be between 1 and 64");
    if (CheckControlCall() != KEPLER_STATUS_OK)
        return KEPLER_STATUS_CALLBACK_CONTEXT;

    try {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (!engine)
            return Fail(KEPLER_STATUS_NOT_INITIALIZED, "call kepler_init first");
        engine->SetThreads(threads);
        return Succeed();
    } catch (const std::exception& exception) {
        return FailFromException(exception);
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unknown exception while setting threads");
    }
}

KeplerStatus KEPLER_CALL kepler_set_multi_pv(int32_t multi_pv) noexcept {
    if (multi_pv < 1 || multi_pv > 218)
        return Fail(KEPLER_STATUS_INVALID_ARGUMENT, "multi_pv must be between 1 and 218");
    if (CheckControlCall() != KEPLER_STATUS_OK)
        return KEPLER_STATUS_CALLBACK_CONTEXT;

    try {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (!engine)
            return Fail(KEPLER_STATUS_NOT_INITIALIZED, "call kepler_init first");
        engine->SetMultiPv(multi_pv);
        return Succeed();
    } catch (const std::exception& exception) {
        return FailFromException(exception);
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unknown exception while setting multi-PV");
    }
}

KeplerStatus KEPLER_CALL kepler_go(int32_t movetime_ms, int32_t depth) noexcept {
    if (movetime_ms < 0)
        return Fail(KEPLER_STATUS_INVALID_ARGUMENT, "movetime_ms must not be negative");
    if (depth < 1 || depth > MAX_DEPTH)
        return Fail(KEPLER_STATUS_INVALID_ARGUMENT, "depth must be between 1 and 30");
    if (CheckControlCall() != KEPLER_STATUS_OK)
        return KEPLER_STATUS_CALLBACK_CONTEXT;

    try {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (!engine)
            return Fail(KEPLER_STATUS_NOT_INITIALIZED, "call kepler_init first");
        if (engine->IsEmptyPosition())
            return Fail(KEPLER_STATUS_NO_POSITION, "call kepler_set_position first");
        if (searching.load(std::memory_order_acquire))
            return Fail(KEPLER_STATUS_SEARCH_RUNNING, "a search is already running");
        engine->Go(movetime_ms, depth);
        return Succeed();
    } catch (const std::exception& exception) {
        return FailFromException(exception);
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unknown exception while starting search");
    }
}

KeplerStatus KEPLER_CALL kepler_go_with_clock(
    int32_t white_time_ms,
    int32_t black_time_ms,
    int32_t white_increment_ms,
    int32_t black_increment_ms,
    int32_t depth
) noexcept {
    if (white_time_ms < 0 || black_time_ms < 0
        || white_increment_ms < 0 || black_increment_ms < 0) {
        return Fail(KEPLER_STATUS_INVALID_ARGUMENT, "clock values must not be negative");
    }
    if (depth < 1 || depth > MAX_DEPTH)
        return Fail(KEPLER_STATUS_INVALID_ARGUMENT, "depth must be between 1 and 30");
    if (CheckControlCall() != KEPLER_STATUS_OK)
        return KEPLER_STATUS_CALLBACK_CONTEXT;

    try {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (!engine)
            return Fail(KEPLER_STATUS_NOT_INITIALIZED, "call kepler_init first");
        if (engine->IsEmptyPosition())
            return Fail(KEPLER_STATUS_NO_POSITION, "call kepler_set_position first");
        if (searching.load(std::memory_order_acquire))
            return Fail(KEPLER_STATUS_SEARCH_RUNNING, "a search is already running");

        const bool white_to_move = engine->GetTurn();
        const int32_t my_time = white_to_move ? white_time_ms : black_time_ms;
        const int32_t opponent_time = white_to_move ? black_time_ms : white_time_ms;
        const int32_t increment = white_to_move ? white_increment_ms : black_increment_ms;
        const int movetime_ms = CalculateMoveTime(
            my_time,
            opponent_time > 0 ? opponent_time : 1,
            increment,
            engine->GetPly()
        );
        engine->Go(movetime_ms, depth);
        return Succeed();
    } catch (const std::exception& exception) {
        return FailFromException(exception);
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unknown exception while starting clocked search");
    }
}

KeplerStatus KEPLER_CALL kepler_stop(void) noexcept {
    if (CheckControlCall() != KEPLER_STATUS_OK)
        return KEPLER_STATUS_CALLBACK_CONTEXT;

    try {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (!engine)
            return Fail(KEPLER_STATUS_NOT_INITIALIZED, "call kepler_init first");
        engine->Stop();
        return Succeed();
    } catch (const std::exception& exception) {
        return FailFromException(exception);
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unknown exception while stopping search");
    }
}

KeplerStatus KEPLER_CALL kepler_new_game(void) noexcept {
    if (CheckControlCall() != KEPLER_STATUS_OK)
        return KEPLER_STATUS_CALLBACK_CONTEXT;

    try {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (!engine)
            return Fail(KEPLER_STATUS_NOT_INITIALIZED, "call kepler_init first");
        if (searching.load(std::memory_order_acquire))
            return Fail(KEPLER_STATUS_SEARCH_RUNNING, "stop the search before starting a new game");
        engine->NewGame();
        return Succeed();
    } catch (const std::exception& exception) {
        return FailFromException(exception);
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unknown exception while starting a new game");
    }
}

KeplerStatus KEPLER_CALL kepler_set_info_callback(
    KeplerInfoCallback callback,
    void* user_data
) noexcept {
    try {
        std::lock_guard<std::mutex> lock(callback_mutex);
        info_callback = {callback, callback ? user_data : nullptr};
        return Succeed();
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unable to register info callback");
    }
}

KeplerStatus KEPLER_CALL kepler_set_best_move_callback(
    KeplerBestMoveCallback callback,
    void* user_data
) noexcept {
    try {
        std::lock_guard<std::mutex> lock(callback_mutex);
        best_move_callback = {callback, callback ? user_data : nullptr};
        return Succeed();
    } catch (...) {
        return Fail(KEPLER_STATUS_INTERNAL_ERROR, "unable to register best-move callback");
    }
}

} // extern "C"
