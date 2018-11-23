#include <iostream>
#include "../parse_arguments.hpp"

using namespace std;

Options process_arguments(int argc, char **argv, bool is_client);

void print_struct_opt(Options opts) {
    cout << "ip:\t" << opts.ip << endl
        << "port:\t" << opts.port << endl
        << "num:\t" << opts.num << endl
        << "block:\t" << opts.block << endl
        << "fork:\t" << opts.fork << endl 
        << endl;
}

int main(int argc, char *argv[]) {
    Options opts;

    cout << "is_client = true\n";
    opts = process_arguments(argc, argv, true);
    print_struct_opt(opts);

    cout << "is_client = false\n";
    opts = process_arguments(argc, argv, false);
    print_struct_opt(opts);

    cout << "test finished\n";

}
