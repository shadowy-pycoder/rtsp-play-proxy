PROJNAME := rtsp_play_proxy
PROJDIR := $(realpath $(CURDIR))
VERSION := $(shell git describe --tags --dirty --always)
BUILD := $(PROJDIR)/build
BIN := $(PROJDIR)/bin
TARGET := rtsp-play-proxy
BINARY := $(BIN)/$(TARGET)
SRC := $(PROJDIR)/src
INCLUDE := $(PROJDIR)/include
LIB := $(PROJDIR)/lib
TESTS := $(PROJDIR)/tests

DEBUG ?= 0
ASAN ?= 0

CC := clang

CFLAGS := -Wall -Wextra -Wno-missing-field-initializers -Wno-missing-braces -pedantic -std=c2x
CPPFLAGS := -I$(INCLUDE) -I$(LIB) -DVERSION=\"$(VERSION)\"
LDFLAGS :=
LDLIBS :=

PREFIX  ?= /usr/local
BINPATH := $(PREFIX)/bin
LIBPATH := $(PREFIX)/lib
INCPATH := $(PREFIX)/include


ifeq ($(DEBUG),1)
  CFLAGS += -g3 -O0
  CPPFLAGS += -DDEBUG
else
  CFLAGS += -O3 -flto -march=native
  LDFLAGS += -flto  -fuse-ld=lld
endif

ifeq ($(ASAN),1)
  CFLAGS += -fsanitize=address,undefined -fsanitize=bounds -fno-omit-frame-pointer
  LDFLAGS += -fsanitize=address,undefined -fsanitize=bounds -fno-omit-frame-pointer
endif

.PHONY: default all clean run compile_commands tests

default: $(BINARY)
all: default

SOURCES := $(wildcard $(SRC)/*.c)
LIB_SOURCES := $(wildcard $(LIB)/*.c)
OBJECTS := $(patsubst $(SRC)/%.c,$(BUILD)/src/%.o,$(SOURCES))
OBJECTS += $(patsubst $(LIB)/%.c,$(BUILD)/lib/%.o,$(LIB_SOURCES))
HEADERS := $(wildcard $(INCLUDE)/*.h)
HEADERS += $(wildcard $(LIB)/*.h)

$(BUILD)/src $(BUILD)/lib:
	mkdir -p $@

$(BIN):
	mkdir -p $@

$(BUILD)/src/%.o: $(SRC)/%.c $(HEADERS) | $(BUILD)/src
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD)/lib/%.o: $(LIB)/%.c $(HEADERS) | $(BUILD)/lib
	$(CC) $(CFLAGS) $(CPPFLAGS) -w -c $< -o $@

$(BINARY): $(OBJECTS) | $(BIN)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

run: $(BIN)/$(TARGET)
	./$(notdir $(BIN))/$(TARGET)

compile_commands:
	bear -- make --always-make all

tests: $(BIN)
	$(CC) -g3 -I$(INCLUDE) $(TESTS)/test_hijack_play_header.c -o $(BIN)/test-hijack-play-header

install:
	install -v -d $${DESTDIR}$(BINPATH)
	install -v -m 0755 $(BINARY) $${DESTDIR}$(BINPATH)/$(TARGET)

uninstall:
	rm -v -f  $${DESTDIR}$(BINPATH)/$(TARGET)

clean:
	rm -rf $(BUILD) $(BIN)
