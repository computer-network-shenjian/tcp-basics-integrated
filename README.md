# tcp-basics-integrated

## Makefile logic

The code base consists of mainly three parts: the ./server and ./client binary executables, `libhw9.so` dynamic library that contains all reusable code, and test cases in `tests/` with file names corresponding to the function each test case targets at.

The makefile generates object files for each library source file before linking them together to avoid the recompilation of code that isn't modified.

## Test Driven Development

This project is designed to be developed with a test driven approach to cope with the potential escalation of the complexity of future computer network projects. The testing api is designed to be as easy to use as possible to lower the mental burden of actually using tests during development.

All tests resides in the `tests/` directory. Running `make tests` in the root directory automatically picks up all `.cpp` files in `tests/` and compile them into test binaries, linking together with the dynamic library `libhw9.so` so you have all library functions available to call and test.

## How to write and run a test

Here is an example writing a test case with _main()_ function and running it. The test case is located at `tests/parse_arguments.cpp` and the make command autocatically finds it so no modification to Makefile is required!

```make
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

[http://www.drdobbs.com/cpp/test-driven-development-in-cc/184401572](http://www.drdobbs.com/cpp/test-driven-development-in-cc/184401572)

[http://alexott.net/en/cpp/CppTestingIntro.html](http://alexott.net/en/cpp/CppTestingIntro.html)

## Notes

### 第二节

1. 将交互内容写成文件，怎么写？（涉及比对，比对的要求是什么“一对同名文件”） -> 见第三章
2. 注意：任何一步收到的数据错误则中断连接，并请求重连
3. 多个连接的处理，既可以是fork子进程方式，也可以由一个主进程处理全部连接，我们怎么做？ -> 都写
4. 每个连接的方式，既可以是阻塞，也可以是非阻塞，我们怎么做？ -> 都写

### 第三节

1. 注意：不准发尾0，处理方法一律strlen
2. 注意：Client发进程所有者的学号是网络序
3. 二进制比较，用diff

### 测试

1. 可以用随机数生成来决定按一定概率的故意传错
2. 限制并发个数，讨论ing -> 队列处理
3. Server写成类mariaDB的二层守护进程（父亲只负责看儿子，儿子做实事）