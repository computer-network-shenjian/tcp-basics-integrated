#include <string>

struct Options {
    std::string ip; // dot seperated ip address
    std::string port; 
    unsigned int num = 100; // number of connections. defaults to 100
    bool block = false; // block/nonblock. defaults to nonblock
    bool fork = false;  // fork/nofork. defaults to nonblock
};

