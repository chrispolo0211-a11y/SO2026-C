CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -pthread -Iinclude
LDLIBS := -lncurses

TARGET := laboratorio_concurrencia
SOURCES := src/main.cpp src/memory.cpp src/ui.cpp src/worker.cpp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDLIBS)

run: $(TARGET)
	./$(TARGET) $(ARGS)

clean:
	rm -f $(TARGET)
