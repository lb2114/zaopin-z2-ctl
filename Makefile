CC = gcc
CFLAGS = -Wall -Wextra -O2
CPPFLAGS = -Isrc $(shell pkg-config --cflags libsystemd)
LDLIBS = $(shell pkg-config --libs libsystemd)

SRC_DIR = src
BUILD_DIR = build
TARGET = $(BUILD_DIR)/zaopin-z2-ctl

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS = $(OBJECTS:.o=.d)

PREFIX = /usr/local
UDEVDIR = /etc/udev/rules.d

.PHONY: all clean

all: $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/zaopinctl
	install -Dm644 udev/70-zaopin-z2.rules $(DESTDIR)$(UDEVDIR)/70-zaopin-z2.rules

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)
