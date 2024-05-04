# Makefile for SidBerry
# LouD 2024

CC = gcc
CXX = g++

# Default build flags (empty)
CPPFLAGS =
CXXFLAGS =
LDLIBS =

# Make/ Make all compile board
# This can be 'FTDI', 'RASPBERRYPI', 'ARIETTAG25' or 'CUSTOM'
BOARD = 'FTDI'

# Out file
TARGET_EXEC := sidberry
TARGET_EXEC_DEBUG := sidberry_debug
TARGET_EXEC_FTDI := sidberry-ftdi
TARGET_EXEC_RASPBERRYPI := sidberry-rpi
TARGET_EXEC_ARIETTAG25 := sidberry-arietta
TARGET_EXEC_CUSTOM := sidberry-custom

BUILD_DIR := ./build
SRC_DIRS := ./src

# Default ldlibs
LDLIBRPI = -lwiringPi
LDLIBARRIETTA = -lwiringSam
LDLIBCUSTOM =

# FTDI ldlibs
LDLIBFTDI = $(shell pkg-config --libs libftdi1)
# FTDI CPP libs
CPPLIBFTDI = $(shell pkg-config --cflags libftdi1)

# Default source files
SRCS := $(shell find $(SRC_DIRS) -name "*.cpp" ! -name "gpio.*" -or -name "*.c" -or -name "*.s")

# FTDI source files and is also used for make/ make all
SRCSFTDI := $(shell find $(SRC_DIRS) -name '*.cpp' -or -name '*.c' -or -name '*.s')
ifeq ($(BOARD), 'FTDI')
DEFAULTSRC := $(SRCSFTDI)
else
DEFAULTSRC := $(SRCS)
endif

# default make
all: default player
# debug
debug: default playerdebug
# board specific
ftdi: default build_ft2232h
raspberrypi: default build_raspberrypi
arietta25g: default build_arietta
custom: default build_custom

# .PHONY: clean
clean:
	rm -f $(BUILD_DIR)/*

default::
	$(info Creating build directory)
	mkdir -p $(BUILD_DIR)

# ftdi build command ~ g++ -g3 $(pkg-config --cflags libftdi1) -o testje *.cpp $(pkg-config --libs libftdi1)
build_ft2232h: CPPFLAGS += -DBOARD='FTDI' $(CPPLIBFTDI)
build_ft2232h: LDLIBS += $(LDLIBFTDI)
build_ft2232h::
		$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(BUILD_DIR)/$(TARGET_EXEC_FTDI) $(SRCSFTDI) $(LDLIBS)

# raspberrypi build command ~ g++ *.cpp -D BOARD='RASPBERRYPI' -o sidberry -lwiringPi
build_raspberrypi: CPPFLAGS += -DBOARD='RASPBERRYPI'
build_raspberrypi: LDLIBS += $(LDLIBRPI)
build_raspberrypi::
			$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(BUILD_DIR)/$(TARGET_EXEC_RASPBERRYPI) $(SRCS) $(LDLIBS)

# arietta build command ~ g++ *.cpp -D BOARD='ARIETTAG25' -o sidberry -lwiringSam
build_arietta: CPPFLAGS += -DBOARD='ARIETTAG25'
build_arietta: LDLIBS += $(LDLIBARRIETTA)
build_arietta::
		$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(BUILD_DIR)/$(TARGET_EXEC_ARIETTAG25) $(SRCS) $(LDLIBS)

# custom build command ~ g++ *.cpp -D BOARD='CUSTOM' -o sidberry
build_custom: CPPFLAGS += -DBOARD='CUSTOM'
build_custom: LDLIBS += $(LDLIBCUSTOM)
build_custom::
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(BUILD_DIR)/$(TARGET_EXEC_CUSTOM) $(SRCS) $(LDLIBS)

## Build sidberry player by BOARD variable
$(info Checking boardtype)
ifeq ($(BOARD), 'FTDI')
player: CPPFLAGS += -DBOARD=${BOARD} $(CPPLIBFTDI)
player: LDLIBS += $(LDLIBFTDI)
playerdebug: CPPFLAGS += -DBOARD=${BOARD} $(CPPLIBFTDI)
playerdebug: LDLIBS += $(LDLIBFTDI)
endif

# Default binary
player: CXXFLAGS += -g3
player::
	$(info Board == $(BOARD))
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(BUILD_DIR)/$(TARGET_EXEC) $(DEFAULTSRC) $(LDLIBS)

# Debug binary
playerdebug: CXXFLAGS += -DDEBUG_SIDBERRY=1
playerdebug: CXXFLAGS += -g3
playerdebug::
		$(info Board == $(BOARD))
		$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(BUILD_DIR)/$(TARGET_EXEC_DEBUG) $(DEFAULTSRC) $(LDLIBS)

## Static binary
# LDFLAGS = -Wl,-Bstatic,--verbose=99,
# playerstatic:
# playerstatic::
