CXX = g++

CXXFLAGS = -std=c++23 -Wall -O3

TARGET = simulator

SRCS = Simulator.cpp Log.cpp FPU.cpp Util.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): main.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.cpp $(SRCS)

debug: main.o $(OBJS)
	$(CXX) $(CXXFLAGS) -DDEBUG -o $(TARGET) main.cpp $(SRCS)

testFPU: testFPU.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) testFPU.cpp $(SRCS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) simulator.log
