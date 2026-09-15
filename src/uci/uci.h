#pragma once

#include <unordered_map>
#include <functional>
#include <string>
#include <vector>

#include "../engine/engine.h"

class Uci {
public:
    using Handler = std::function<void(const std::vector<std::string>&)>;
    Uci();
    ~Uci();

    void Execute(const std::vector<std::string>& parsed_command);
private:
    std::unordered_map<std::string, Handler> handlers_;
    Engine engine_;
};