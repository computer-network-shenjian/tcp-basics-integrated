# tcp-basics-integrated

## Makefile logic
The code base consists of mainly three parts: the ./server and ./client binary executables, `libhw9.so` dynamic library that contains all reusable code, and test cases in `tests/` with the same file names as the function each test case targets at.

The makefile generates object files for each library source file before linking them together to avoid the recompilation of code that isn't modified.

## Test Driven Development
This project is designed to be developed with a test driven approach to cope with the potential escalation of the complexity of future computer network projects. The testing api is designed to be as easy to use as possible to lower the mental burden of actually using tests during development.

All tests resides in the `tests/` directory. Running `make tests` in the root directory automatically picks up all `.cpp` files in `tests/` and compile them into test binaries, linking together with the dynamic library `libhw9.so` so you have all library functions available to call and test.

## How to write and run a test
Here is an example writing a test case with _main()_ function and running it. The test case is located at `tests/parse_arguments.cpp` and the make command autocatically finds it so no modification to Makefile is required!

```
$ make tests && LD_LIBRARY_PATH=. tests/parse_arguments --ip 1.1.1.1 --port 12 --block --fork  --num 110
g++ -std=c++11 -fPIC -Wall -Wextra   -c -o parse_arguments.o parse_arguments.cpp
g++ -o libhw9.so parse_arguments.o parse_arguments.hpp -shared -std=c++11 -fPIC -Wall -Wextra
g++ -std=c++11 -fPIC -Wall -Wextra -L. -lhw9 -o tests/parse_arguments tests/parse_arguments.cpp
is_client = true
ip:     1.1.1.1
port:   12
num:    110
block:  1
fork:   1

is_client = false
ip:     1.1.1.1
port:   12
num:    110
block:  1
fork:   1

test finished

```

You see the elegance of test driven development? It's as easy as that. Please write rigid tests when possible!

### More about TDD
![http://www.drdobbs.com/cpp/test-driven-development-in-cc/184401572](http://www.drdobbs.com/cpp/test-driven-development-in-cc/184401572)
![http://alexott.net/en/cpp/CppTestingIntro.html](http://alexott.net/en/cpp/CppTestingIntro.html)
