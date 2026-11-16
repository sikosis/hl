NAME := hl
VERSION := 0.1.0

CXX ?= c++
CXXFLAGS ?= -O2
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -pthread
CPPFLAGS := -Isrc -DHL_VERSION=\"$(VERSION)\"
LDFLAGS := -pthread
LIBS := -lbe

PREFIX ?= /boot/home/config/non-packaged
BINDIR ?= $(PREFIX)/bin

SOURCES := \
	src/main.cpp \
	src/config/config.cpp \
	src/format/formatter.cpp \
	src/io/entry_splitter.cpp \
	src/io/input.cpp \
	src/io/pager.cpp \
	src/model/level.cpp \
	src/model/record.cpp \
	src/parse/format.cpp \
	src/parse/json_scanner.cpp \
	src/parse/logfmt_scanner.cpp \
	src/parse/scanner.cpp \
	src/parse/timestamp.cpp \
	src/pipeline/processor.cpp \
	src/platform/haiku/platform_haiku.cpp \
	src/query/filter.cpp \
	src/query/json_path.cpp \
	src/query/query.cpp \
	src/query/time_argument.cpp

OBJECTS := $(SOURCES:.cpp=.o)
CORE_OBJECTS := $(filter-out src/main.o,$(OBJECTS))
DEPENDENCIES := $(OBJECTS:.o=.d)
TEST_SOURCES := $(wildcard tests/*_test.cpp)
TEST_BINARIES := $(TEST_SOURCES:.cpp=)

.PHONY: all test install clean

all: $(NAME)

$(NAME): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

$(OBJECTS): %.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

tests/%_test: tests/%_test.cpp $(CORE_OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(CORE_OBJECTS) $(LIBS) -o $@

test: $(NAME) $(TEST_BINARIES)
	@./$(NAME) --version | grep -F "hl $(VERSION)" >/dev/null
	@./$(NAME) --help | grep -F "Usage: hl" >/dev/null
	@set -e; for test_binary in $(TEST_BINARIES); do \
		echo "Running $$test_binary"; \
		"$$test_binary"; \
	done

install: $(NAME)
	mkdir -p "$(DESTDIR)$(BINDIR)"
	cp $(NAME) "$(DESTDIR)$(BINDIR)/$(NAME)"
	chmod 755 "$(DESTDIR)$(BINDIR)/$(NAME)"

clean:
	rm -f $(OBJECTS) $(DEPENDENCIES) $(TEST_BINARIES) $(NAME)
	rm -rf build

-include $(DEPENDENCIES)
