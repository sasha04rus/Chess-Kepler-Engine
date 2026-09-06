#include "uci.h"

#include <vector>
#include <string>

#include "../moves/to_string.h"
#include "../search/search.h"
#include "../time/time_manager.h"
#include "../tt/tt.h"

static const std::string kStartPosFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

Uci::Uci() {
    engine_.SetInfoCallback([](const SearchInfo& info) {
        std::cout << "info depth " << info.depth << " score ";
        if (info.mate_in != 0)
            std::cout << " mate " << info.mate_in;
        else std::cout << " cp " << info.evaluation;
        std::cout << " time " << info.time << " nodes " << info.nodes << " nps " << info.nps << " pv ";
        for (const auto& move : info.pv)
            std::cout << move << ' ';
        std::cout << std::endl;
    });

    engine_.SetBestMoveCallback([](const std::string& move) {
        std::cout << "bestmove " << move << std::endl;
    });

    handlers_["uci"] = [](const std::vector<std::string>& args) {
        if (!args.empty()) return;
        std::cout << "id name Kepler-Engine v0\nid author Sasha Tastakov\n"
            << "option name MultiPV type spin default 1 min 1 max 218\n"
            << "option name Threads type spin default 1 min 1 max 64\n"
            << "uciok"
            << std::endl;
    };

    handlers_["isready"] = [](const std::vector<std::string>& args) {
        if (!args.empty()) return;
        std::cout << "readyok" << std::endl;
    };

    handlers_["ucinewgame"] = [this](const std::vector<std::string>& args) {
        if (!args.empty())
            return;
        engine_.NewGame();
    };

    handlers_["setoption"] = [this](const std::vector<std::string>& args) {
        if (args.size() != 4) return;
        if ((args[0] != "name") || (args[2] != "value")) return;
        try {
            if (args[1] == "MultiPV")
                engine_.SetMultiPv(std::clamp(std::stoi(args[3]), 1, 218));
            else if (args[1] == "Threads")
                engine_.SetThreads(std::clamp(std::stoi(args[3]), 1, 64));
        } catch(...) { return; }
    };
    
    handlers_["position"] = [this](const std::vector<std::string>& args) {
        if (args.empty()) return;
        std::size_t i = 1;
        if (args[0] == "startpos")
            engine_.SetPosition(kStartPosFen);
        else if (args[0] == "fen") {
            if (args.size() < 7) return;
            std::string fen = args[1] + ' ' + args[2] + ' ' + args[3] + ' ' + args[4] + ' ' + args[5] + ' ' + args[6];
            engine_.SetPosition(fen);
            i = 7;
        } else return;
        if (i < args.size() && args[i++] == "moves") {
            for (; i < args.size(); i++) 
                engine_.MakeMove(args[i]);
        }
    };

    handlers_["go"] = [this](const std::vector<std::string>& args) {
        if (searching) return;
        if (engine_.IsEmptyPosition()) {
            std::cout << "empty position" << std::endl;
            return;
        }
        int move_time = 0;
        int depth = MAX_DEPTH;
        try {
            if (args.empty() || args[0] == "infinity") {
                move_time = 0;
            } else if (args.size() == 2 && args[0] == "movetime") {
                move_time = std::stoi(args[1]);
            } else if (args.size() == 2 && args[0] == "depth") {
                depth = std::stoi(args[1]);
            } else if (args.size() >= 4) {
                if ((args[0] != "wtime") || (args[2] != "btime")) return;
                int wtime = std::stoi(args[1]);
                int btime = std::stoi(args[3]);
                int winc = args.size() > 5 ? std::stoi(args[5]) : 0;
                if (engine_.GetTurn())
                    move_time = CalculateMoveTime(wtime, btime, winc, engine_.GetPly());
                else move_time = CalculateMoveTime(btime, wtime, winc, engine_.GetPly());
            } else return;
        } catch(...) { return; }
        engine_.Go(move_time, depth);
    };

    handlers_["stop"] = [this](const std::vector<std::string>& args) {
        if (!args.empty()) return;
        engine_.Stop();
    };

    handlers_["quit"] = [this](const std::vector<std::string>& args) {
        if (!args.empty()) return;
        engine_.Stop();
    };
}

void Uci::Execute(const std::vector<std::string>& parsed_command) {
    if (parsed_command.empty()) return;
    auto it = handlers_.find(parsed_command[0]);
    if (it == handlers_.end()) return;
    it->second(std::vector<std::string>(parsed_command.begin() + 1, parsed_command.end()));
}