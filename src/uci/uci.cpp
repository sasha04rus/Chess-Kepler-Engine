#include "uci.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "../search/search.h"
#include "../time/time_manager.h"

namespace {

constexpr const char* kStartPosFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

std::mutex output_mutex;

void EmitLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(output_mutex);
    std::cout << line << std::endl;
}

bool ParseInt(const std::string& text, int& value) {
    try {
        std::size_t parsed = 0;
        const long long result = std::stoll(text, &parsed);
        if (parsed != text.size() || result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max())
            return false;
        value = static_cast<int>(result);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

Uci::Uci() {
    engine_.SetInfoCallback([](const SearchInfo& info) {
        std::ostringstream output;
        output << "info depth " << info.depth << " score ";
        if (info.mate_in != 0)
            output << "mate " << info.mate_in;
        else
            output << "cp " << info.evaluation;
        output << " time " << info.time << " nodes " << info.nodes << " nps " << info.nps << " pv";
        for (const auto& move : info.pv)
            output << ' ' << move;
        EmitLine(output.str());
    });

    engine_.SetBestMoveCallback([](const std::string& move) {
        EmitLine("bestmove " + move);
    });

    handlers_["uci"] = [](const std::vector<std::string>& args) {
        if (!args.empty())
            return;
        EmitLine(
            "id name Kepler-Engine Standard v0\n"
            "id author Sasha Tastakov\n"
            "option name MultiPV type spin default 1 min 1 max 218\n"
            "option name Threads type spin default 1 min 1 max 64\n"
            "uciok"
        );
    };

    handlers_["isready"] = [](const std::vector<std::string>& args) {
        if (args.empty())
            EmitLine("readyok");
    };

    handlers_["ucinewgame"] = [this](const std::vector<std::string>& args) {
        if (args.empty())
            engine_.NewGame();
    };

    handlers_["setoption"] = [this](const std::vector<std::string>& args) {
        if (args.size() != 4 || args[0] != "name" || args[2] != "value")
            return;

        int value = 0;
        if (!ParseInt(args[3], value))
            return;
        if (args[1] == "MultiPV")
            engine_.SetMultiPv(std::clamp(value, 1, 218));
        else if (args[1] == "Threads")
            engine_.SetThreads(std::clamp(value, 1, 64));
    };

    handlers_["position"] = [this](const std::vector<std::string>& args) {
        if (args.empty())
            return;

        std::size_t index = 0;
        if (args[index] == "startpos") {
            engine_.SetPosition(kStartPosFen);
            index++;
        } else if (args[index] == "fen") {
            index++;
            if (args.size() - index < 6)
                return;
            std::ostringstream fen;
            for (int field = 0; field < 6; field++) {
                if (field != 0)
                    fen << ' ';
                fen << args[index++];
            }
            engine_.SetPosition(fen.str());
        } else return;
        if (index == args.size())
            return;
        if (args[index++] != "moves")
            return;
        for (; index < args.size(); index++)
            engine_.MakeMove(args[index]);
    };

    handlers_["go"] = [this](const std::vector<std::string>& args) {
        if (searching || engine_.IsEmptyPosition())
            return;

        int depth = MAX_DEPTH;
        int move_time = -1;
        int white_time = -1;
        int black_time = -1;
        int white_increment = 0;
        int black_increment = 0;

        for (std::size_t index = 0; index < args.size(); index++) {
            const std::string& name = args[index];
            if (name == "infinite" || name == "infinity" || name == "ponder")
                continue;
            if (name == "searchmoves")
                break;
            if (index + 1 >= args.size())
                return;

            int value = 0;
            if (!ParseInt(args[index++], value))
                return;
            if (name == "depth")
                depth = value;
            else if (name == "movetime")
                move_time = value;
            else if (name == "wtime")
                white_time = value;
            else if (name == "btime")
                black_time = value;
            else if (name == "winc")
                white_increment = value;
            else if (name == "binc")
                black_increment = value;
        }

        if (move_time < 0 && white_time >= 0 && black_time >= 0) {
            if (engine_.GetTurn())
                move_time = CalculateMoveTime(white_time, black_time, white_increment, engine_.GetPly());
            else
                move_time = CalculateMoveTime(black_time, white_time, black_increment, engine_.GetPly());
        }
        engine_.Go(std::max(move_time, 0), std::clamp(depth, 1, MAX_DEPTH));
    };

    handlers_["stop"] = [this](const std::vector<std::string>& args) {
        if (args.empty())
            engine_.Stop();
    };

    handlers_["quit"] = [this](const std::vector<std::string>& args) {
        if (args.empty())
            engine_.Stop();
    };
}

Uci::~Uci() {
    engine_.Stop();
}

void Uci::Execute(const std::vector<std::string>& parsed_command) {
    if (parsed_command.empty())
        return;
    const auto handler = handlers_.find(parsed_command[0]);
    if (handler == handlers_.end())
        return;
    handler->second(std::vector<std::string>(parsed_command.begin() + 1, parsed_command.end()));
}