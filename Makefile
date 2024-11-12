CXX = g++

CXXFLAGS = -std=c++23 -Wall -O3

TARGET = simulator

SRCS = Simulator.cpp Log.cpp FPU.cpp Option.cpp Util.cpp Memory.cpp

OBJS = $(SRCS:.cpp=.o)

DEPS = $(SRCS:.cpp=%.d)

all: $(TARGET)
	ulimit -s unlimited

-include $(DEPS)	

$(TARGET): main.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.cpp $(SRCS)

debug: main.o $(OBJS)
	$(CXX) $(CXXFLAGS) -DDEBUG -o $(TARGET) main.cpp $(SRCS)

testFPU: testFPU.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) testFPU.cpp $(SRCS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -MMD -MP $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(DEPS) *.log *.err
