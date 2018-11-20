#include <string>

struct Options {
    int num_options = 4;

    std::string ip; // dot seperated ip address
    std::string port; 
    unsigned int num = 100; // number of connections. defaults to 100
    bool block = false; // block/nonblock. defaults to nonblock
    bool fork = false;  // fork/nofork. defaults to nonblock
};

Options process_arguments(int argc, char **argv, bool is_client);
