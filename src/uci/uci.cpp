#include "uci.h"

#include <vector>
#include <string>

#include "../moves/to_string.h"
#include "../search/search.h"
#include "../time/time_manager.h"
#include "../tt/tt.h"

static const std::string kStartPosFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

Uci::Uci() {
    handlers_["uci"] = [](const std::vector<std::string>& args, std::ostream& out) {
        if (!args.empty()) return;
        out << "id name Kepler-Engine v0\nid author Sasha Tastakov\n"
            << "option name MultiPV type spin default 1 min 1 max 218\n"
            << "option name Threads type spin default 4 min 1 max 64\n"
            << "uciok"
            << std::endl;
    };

    handlers_["isready"] = [](const std::vector<std::string>& args, std::ostream& out) {
        if (!args.empty()) return;
        out << "readyok" << std::endl;
    };

    handlers_["setoption"] = [this](const std::vector<std::string>& args, std::ostream& out) {
        if (args.size() != 4) return;
        if ((args[0] != "name") || (args[2] != "value")) return;
        try {
            if (args[1] == "MultiPV")
                multi_pv_ = std::stoi(args[3]);
            else if (args[1] == "Threads")
                threads_ = std::stoi(args[3]);
        } catch(...) { return; }
    };

    handlers_["variant"] = [this](const std::vector<std::string>& args, std::ostream& out) {
        if (args[0] == "standard")
            variant_ = Variant::kStandard;
        else if (args[0] == "tar")
            variant_ = Variant::kTakeAndReturn;
    };
    
    handlers_["position"] = [this](const std::vector<std::string>& args, std::ostream& out) {
        if (args.empty()) return;
        if (searching) {
            stop_signal = true;
            pthread_join(search_thread_, nullptr);
            searching = false;
        }
        board_ptr_.reset();
        std::size_t i = 1;
        if (args[0] == "startpos")
            board_ptr_ = std::make_unique<Board>(kStartPosFen);
        else if (args[0] == "fen") {
            if (args.size() < 7) return;
            std::string fen = args[1] + ' ' + args[2] + ' ' + args[3] + ' ' + args[4] + ' ' + args[5] + ' ' + args[6];
            board_ptr_ = std::make_unique<Board>(fen);
            i = 7;
        } else return;
        if (i < args.size() && args[i++] == "moves") {
            for (; i < args.size(); i++) 
                board_ptr_->MakeMove(args[i], variant_);
        }
    };

    handlers_["go"] = [this](const std::vector<std::string>& args, std::ostream& out) {
        if (searching) return;
        if (!board_ptr_) {
            out << "empty position";
            return;
        }
        try {
            if (args.empty() || args[0] == "infinity") {
                movetime = 0;
            } else if (args.size() == 2 && args[0] == "movetime") {
                movetime = std::stoi(args[1]);
            } else if (args.size() >= 4) {
                if ((args[0] != "wtime") || (args[2] != "btime")) return;
                int wtime = std::stoi(args[1]);
                int btime = std::stoi(args[3]);
                int winc = args.size() > 5 ? std::stoi(args[5]) : 0;
                if (board_ptr_->turn)
                    movetime = CalculateMoveTime(wtime, btime, winc, board_ptr_->ply);
                else movetime = CalculateMoveTime(btime, wtime, winc, board_ptr_->ply);
            } else return;
        } catch(...) { return; }
        stop_signal = false;
        searching = true;
        ClearTT();
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        SearchArgs* sa = new SearchArgs{*board_ptr_, depth_, threads_, multi_pv_};
        if (variant_ == Variant::kStandard)
            pthread_create(&search_thread_, nullptr, Search<Move>, sa);
        else if (variant_ == Variant::kTakeAndReturn)
            pthread_create(&search_thread_, nullptr, Search<Move>, sa);
    };

    handlers_["stop"] = [this](const std::vector<std::string>& args, std::ostream& out) {
        if (!args.empty()) return;
        if (searching) {
            stop_signal = true;
            pthread_join(search_thread_, nullptr);
            searching = false;
            out << bestmove;
        }
        board_ptr_.reset();
    };

    handlers_["quit"] = [this](const std::vector<std::string>& args, std::ostream& out) {
        if (!args.empty()) return;
        if (searching) {
            stop_signal = true;
            pthread_join(search_thread_, nullptr);
            searching = false;
        }
        board_ptr_.reset();
    };
}

void Uci::Execute(const std::vector<std::string>& parsed_command, std::ostream& out) {
    if (parsed_command.empty()) return;
    auto it = handlers_.find(parsed_command[0]);
    if (it == handlers_.end()) return;
    it->second(std::vector<std::string>(parsed_command.begin() + 1, parsed_command.end()), out);
}