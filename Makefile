.DEFAULT_GOAL := all
CXX := g++
CPPFLAGS ?=
CXXFLAGS ?= -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?=
STANDARD := -std=c++17 -pedantic-errors
PYTHON ?= python3
TARGET := main
SOURCES := main.cpp model.cpp date.cpp policy.cpp catalog.cpp readers.cpp circulation.cpp reservations.cpp reports.cpp storage.cpp web_view.cpp recommendations.cpp
OBJECTS := $(SOURCES:%.cpp=build/%.o)

.PHONY: all clean test demo web web-demo web-test
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
web: $(TARGET)
	$(PYTHON) -B web/server.py
web-demo: $(TARGET)
	$(PYTHON) -B web/server.py --demo
web-test: $(TARGET)
	$(PYTHON) -B tests/web_tests.py ./$(TARGET)
clean:
	rm -rf build $(TARGET)

-include $(OBJECTS:.o=.d)
