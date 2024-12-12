CXX = g++
CXXFLAGS = -std=c++23 -Wall -O3
TARGET = simulator
SRCDIR = src
OBJDIR = build
SRCS = $(SRCDIR)/Simulator.cpp $(SRCDIR)/Log.cpp $(SRCDIR)/FPU.cpp $(SRCDIR)/Option.cpp $(SRCDIR)/Util.cpp $(SRCDIR)/Memory.cpp
OBJS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRCS))
DEPS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.d, $(SRCS)) $(OBJDIR)/main.d $(OBJDIR)/testFPU.d

all: $(TARGET)
	ulimit -s unlimited

-include $(DEPS)

$(TARGET): $(OBJDIR)/main.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJDIR)/main.o $(OBJS)

debug: $(OBJDIR)/main.o $(OBJS)
	$(CXX) $(CXXFLAGS) -DDEBUG -o $(TARGET) $(OBJDIR)/main.o $(OBJS)

testFPU: $(OBJDIR)/testFPU.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o testFPU $(OBJDIR)/testFPU.o $(OBJS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -MMD -MP $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET) *.log *.err testFPU main.o testFPU.o
