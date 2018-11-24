CXXFLAGS := -std=c++11 -fPIC -Wall -Wextra

SRC_SERVER := server.cpp
SRC_CLIENT := client.cpp

TARGET_SERVER := $(SRC_SERVER:.cpp=)
TARGET_CLIENT := $(SRC_CLIENT:.cpp=)


TARGET_LIB := libg3.so


SRC_TESTS := $(wildcard tests/*.cpp)
TARGET_TESTS := $(patsubst tests/%.cpp,tests/%,$(SRC_TESTS))

.tests: $(TARGET_TESTS) # prepend a dot to avoid setting default

all: $(TARGET_LIB) $(TARGET_SERVER) $(TARGET_CLIENT) .tests
notest: $(TARGET_LIB) $(TARGET_SERVER) $(TARGET_CLIENT)


####### binary section
$(TARGET_SERVER): $(TARGET_LIB)
$(TARGET_CLIENT): $(TARGET_LIB)

####### library section
SRC_LIB := parse_arguments.cpp shared_library.cpp
HEADER_LIB := $(SRC_LIB:.cpp=.hpp)
OBJ_LIB := $(SRC_LIB:.cpp=.o)
LIB_FLAGS := -L. -lg3

$(TARGET_LIB): $(OBJ_LIB) $(HEADER_LIB)
	$(CXX) $(LDFLAGS) $(CXXFLAGS) -shared -o $@ $^


######## tests section
tests/%: tests/%.cpp $(TARGET_LIB)
	$(CXX) $(CXXFLAGS) $(LIB_FLAGS) -o $@ $<


######## misc
RM := rm -f
.PHONY : clean all tests
clean :
	$(RM) $(TARGET_SERVER) $(TARGET_CLIENT)
	$(RM) $(OBJ_LIB) $(TARGET_LIB)
	$(RM) $(OBJ_TESTS) $(TARGET_TESTS)
