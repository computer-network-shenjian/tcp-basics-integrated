CXXFLAGS = -std=c++11 -fPIC -Wall -Wextra
LDFLAGS = -shared

SRC_SERVER = server.cpp
SRC_CLIENT = client.cpp

TARGET_SERVER = $(SRC_SERVER:.cpp=)
TARGET_CLIENT = $(SRC_CLIENT:.cpp=)

all: $(TARGET_LIB) $(TARGET_SERVER) $(TARGET_CLIENT) $(tests)

# library section
SRC_LIB = parse_options.cpp
OBJ_LIB = $(SRC_LIB:.cpp=.o)
TARGET_LIB = libhw9.so
LIB_FLAGS = -L. -lhw9

$(TARGET_LIB): $(OBJ_LIB)
	$(CXX) -o $@ $^ $(LDFLAGS)

# tests
tests: tests/process_arguments
tests/process_arguments: tests/process_arguments.cpp $(TARGET_LIB)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIB_FLAGS)

.PHONY : clean all tests
clean :
	-rm $(TARGET_CLIENT) $(TARGET_SERVER) $(TARGET_LIB) $(OBJ_LIB)
