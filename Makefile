CXX = g++
CXXFLAGS = -std=c++23 -Wall -O3 -march=native
LDFLAGS = -lssl -lcrypto
TARGET = simulator
SRCDIR = src
OBJDIR = build
SRCS = $(SRCDIR)/Simulator.cpp $(SRCDIR)/Log.cpp $(SRCDIR)/FPU.cpp $(SRCDIR)/Util.cpp $(SRCDIR)/Memory.cpp $(SRCDIR)/DiscordNotifier.cpp $(SRCDIR)/OptionHandler.cpp $(SRCDIR)/Predictor.cpp
OBJS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRCS))
CXXFLAGS += -Iinclude

# 依存ファイル
DEPS = $(OBJS:.o=.d) $(OBJDIR)/main.d $(OBJDIR)/testFPU.d

all: $(TARGET)
	ulimit -s unlimited

-include $(DEPS)

$(TARGET): $(OBJDIR)/main.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJDIR)/main.o $(OBJS) $(LDFLAGS)

profile: CXXFLAGS += -pg
profile: LDFLAGS += -pg
profile: clean $(TARGET)

debug: $(OBJDIR)/main.o $(OBJS)
	$(CXX) $(CXXFLAGS) -DDEBUG -o $(TARGET) $(OBJDIR)/main.o $(OBJS) $(LDFLAGS)

testFPU: $(OBJDIR)/testFPU.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o testFPU $(OBJDIR)/testFPU.o $(OBJS) $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -MMD -MP $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET) *.log *.err testFPU gmon.out
