#include <string>
#include <sstream>
#include <vector>

#include "hashing/zobrist.h"
#include "uci/uci.h"

int main() {
    zobrist::Init();
    Uci uci;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::vector<std::string> words;
        std::string word;
        while (iss >> word) {
            words.push_back(word);
        }
        uci.Execute(words, std::cout);
        if ((words.size() == 1) && words[0] == "quit")
            return 0;
    }
    return 0;
}