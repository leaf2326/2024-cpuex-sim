CXX = g++

CXXFLAGS = -std=c++17 -Wall -O3

TARGET = simulator

SRCS = main.cpp Simulator.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

debug: $(OBJS)
	$(CXX) $(CXXFLAGS) -DDEBUG -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) 
