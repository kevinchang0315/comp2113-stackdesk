.DEFAULT_GOAL := all
CXX := g++
CPPFLAGS ?=
CXXFLAGS ?= -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?=
STANDARD := -std=c++17 -pedantic-errors
PYTHON ?= python3
TARGET := main
SOURCES := main.cpp model.cpp date.cpp policy.cpp catalog.cpp readers.cpp circulation.cpp reservations.cpp reports.cpp storage.cpp
OBJECTS := $(SOURCES:%.cpp=build/%.o)

.PHONY: all clean test demo
all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@
build/%.o: %.cpp
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(STANDARD) $(CXXFLAGS) -MMD -MP -c $< -o $@
test: $(TARGET)
	$(PYTHON) tests/system_tests.py ./$(TARGET)
demo: $(TARGET)
	sh demo/demo.sh ./$(TARGET)
clean:
	rm -rf build $(TARGET)

-include $(OBJECTS:.o=.d)
