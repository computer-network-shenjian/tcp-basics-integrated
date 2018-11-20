#include <vector>
#include <iostream>
#include "parse_options.hpp"

void print_help(bool is_client) {
    std::cout << "Usage: " << (is_client ? "client" : "server") << " --ip x.x.x.x"
       << " --port xx" << " --block/--nonblock" << "--fork/--nonfork" 
       << (is_client ? " [--num 1~1000]" : "") << std::endl;
}

Options process_arguments(int argc, char **argv, bool is_client=false) {
    std::vector<std::string> arguments;
    Options opts;
    for (int i = 1; i < argc; ++i)
        arguments.push_back(argv[i]);

    for (auto argument: arguments) {
        auto &&arg = argument;
        std::cout << arg << std::endl;
    }

    return opts;
}

