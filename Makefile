CXX = g++
CXXFLAGS = -std=c++23 -Wall -O3
TARGET = simulator
SRCS = Simulator.cpp Log.cpp FPU.cpp Option.cpp Util.cpp Memory.cpp
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d) main.d testFPU.d

all: $(TARGET)
	ulimit -s unlimited

-include $(DEPS)

$(TARGET): main.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.o $(OBJS)

debug: main.o $(OBJS)
	$(CXX) $(CXXFLAGS) -DDEBUG -o $(TARGET) main.o $(OBJS)

testFPU: testFPU.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o testFPU testFPU.o $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -MMD -MP $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(DEPS) *.log *.err testFPU
