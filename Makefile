CXXFLAGS = -std=c++11

SRC_SERVER = server.cpp
SRC_CLIENT = client.cpp
TARGET_SERVER = $(SRC_SERVER:.cpp=)
TARGET_CLIENT = $(SRC_CLIENT:.cpp=)

all: $(TARGET_SERVER) $(TARGET_CLIENT)

.PHONY : clean all
clean :
	-rm $(TARGET_CLIENT) $(TARGET_SERVER)
