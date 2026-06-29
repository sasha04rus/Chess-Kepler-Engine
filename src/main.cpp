#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <unistd.h>

#include "board/board.h"
#include "search/search.h"
#include "hashing/zobrist.h"
#include "time/time_manager.h"
#include "movegen/generate_moves.h"
#include "tt/tt.h"
#include "eval/standard/evaluate_position.h"

static const std::string kStartPosFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

int main() {
    zobrist::Init();
    bool from_startpos = false;
    Variant variant = Variant::kStandard;
    pthread_t search_thread;
    std::string line;
    Board* board = nullptr;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::vector<std::string> words;
        std::string word;
        while (iss >> word) {
            words.push_back(word);
        }
        if (words.empty())
            continue;  

        word = words[0];
        if (word == "uci") {
            std::cout << "id name sasha04rusEngine v1.2\nid author Sasha Tastakov\n"
                        << "option name MultiPV type spin default 1 min 1 max 218\n"
                        << "option name Threads type spin default 4 min 1 max 64\n"
                        << "uciok"
                        << std::endl;
        } else if (word == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (word == "position") {
            if (searching) {
                stop_signal = true;
                pthread_join(search_thread, nullptr);
                searching = false;
            }
            delete board;
            board = nullptr;

            int i = 2;
            if (words[1] == "startpos") {
                board = new Board(kStartPosFen);
                from_startpos = true;
            } else if (words[1] == "fen") {
                from_startpos = false;
                std::string fen = words[2] + ' ' + words[3] + ' ' + words[4] + ' ' + words[5] + ' ' + words[6] + ' ' + words[7];
                board = new Board(fen);
                i = 8;
            } else {
                continue;
            }

            if (i < words.size() && words[i++] == "moves") {
                for (i; i < words.size(); ++i) 
                    board->MakeMove(words[i], variant);
            }
        } else if (word == "go") {
            if (searching)
                continue;
            ClearTT();

            stop_signal = false;
            movetime = 0;

            if (words.size() == 3 && words[1] == "movetime") {
                movetime = stoi(words[2]);
            } else if (words.size() >= 5) {
                int wtime = stoi(words[2]);
                int btime = stoi(words[4]);
                int winc = words.size() > 6 ? stoi(words[6]) : 0;
                if (board->turn)
                    movetime = CalculateMoveTime(wtime, btime, winc, board->ply);
                else
                    movetime = CalculateMoveTime(btime, wtime, winc, board->ply);
            }
            clock_gettime(CLOCK_MONOTONIC, &start_time);
            int num_threads = (threads > std::thread::hardware_concurrency()) ? std::thread::hardware_concurrency() : threads;
            std::vector<pthread_t> threads(num_threads);
            std::vector<ThreadResult<Move>> results(num_threads);
            for (int i = 0; i < num_threads; i++) {
                SearchArgs<Move>* args = new SearchArgs<Move>(*board);
                args->thread_id = i;
                args->result = &results[i];
                args->multi_pv = 1;
                if (variant == Variant::kStandard)
                    pthread_create(&threads[i], nullptr, Search<Move>, args);
                else
                    pthread_create(&threads[i], nullptr, Search<MoveTar>, args);
            }
            searching = true;
            struct timespec current_time;
            int elapsed_time = 0;
    
            while (searching && !stop_signal) {
                usleep(100);
        
                clock_gettime(CLOCK_MONOTONIC, &current_time);
                elapsed_time = (current_time.tv_sec - start_time.tv_sec) * 1000 + (current_time.tv_nsec - start_time.tv_nsec) / 1000000;                
                if (movetime > 0 && elapsed_time >= movetime) {
                    stop_signal = true;
                }
            }
        
            for (auto& th : threads) {
                pthread_join(th, nullptr);
            }
        
            Move best_move;
            int best_depth = -1;
            int best_score = -INFINITY;
        
            for (const auto& res : results) {
                if (res.finished) {
                    if (res.depth > best_depth || 
                        (res.depth == best_depth && res.score > best_score)) {
                        best_depth = res.depth;
                        best_score = res.score;
                        best_move = res.best_move;
                    }
                }
            }
        
            if (best_depth == -1 && results.size() > 0) {
                best_move = results[0].best_move;
            }
        
            std::cout << "bestmove " << MoveToString(best_move) << std::endl;        
            searching = false;
        } else if (word == "stop") {
            if (searching) {
                stop_signal = true;
                pthread_join(search_thread, nullptr);
                searching = false;
            }
            delete board;
            board = nullptr;
        } else if (word == "quit") {
            if (searching) {
                stop_signal = true;
                pthread_join(search_thread, nullptr);
                searching = false;
            }
            delete board;
            board = nullptr;
            break;
        }
    }

    return 0;
}