# Makefile for SidBerry
# LouD 2024

CC = gcc
CXX = g++

# Default build flags (empty)
CPPFLAGS = -g3
CXXFLAGS =
LDLIBS =

# Make default build and debug build
# This can be 'usbsidpico', 'ftdi', 'raspberrypi', 'arietta25g' or 'custom'
DEFAULT = usbsidpico
.DEFAULT_GOAL := $(DEFAULT)
DEBUG := $(DEFAULT)

# Out file
TARGET_EXEC_USBSIDPICO := sidberry-usbsidpico
TARGET_EXEC_FTDI := sidberry-ftdi
TARGET_EXEC_RASPBERRYPI := sidberry-rpi
TARGET_EXEC_ARIETTAG25 := sidberry-arietta
TARGET_EXEC_CUSTOM := sidberry-custom

BUILD_DIR := ./build
SRC_DIRS := ./src

# Default cpplibs per target
CPPLIBUSBSIDPICO = $(shell pkg-config --cflags libusb-1.0)
CPPLIBFTDI = $(shell pkg-config --cflags libftdi1)
CPPLIBRPI =
CPPLIBARIETTA =
CPPLIBCUSTOM =

# Default ldlibs per target
LDLIBUSBSIDPICO = $(shell pkg-config --libs libusb-1.0)
LDLIBFTDI = $(shell pkg-config --libs libftdi1)
LDLIBRPI = -lwiringPi
LDLIBARRIETTA = -lwiringSam
LDLIBCUSTOM =

# Default source files
SRCS := $(shell find $(SRC_DIRS) -name "*.cpp" ! -name "gpio.*" -or -name "*.c" -or -name "*.s")

# FTDI source files need gpio.h and gpio.c
SRCSFTDI := $(shell find $(SRC_DIRS) -name '*.cpp' -or -name '*.c' -or -name '*.s')

# make all
all: usbsidpico ftdi raspberrypi arietta25g
usb: usbsidpico ftdi
# debug
debug: CPPFLAGS += -DDEBUG_SIDBERRY=1 -DLIBUSB_DEBUG=1
debug: $(DEBUG)
# board specific
usbsidpico: default build_usbsidpico
ftdi: default build_ft2232h
raspberrypi: default build_raspberrypi
arietta25g: default build_arietta
custom: default build_custom

clean:
	rm -f $(BUILD_DIR)/*

default::
	$(info Creating build directory)
	mkdir -p $(BUILD_DIR)

$(info Checking boardtype)

# usbsidpico build command ~ g++ -g3 o testje *.cpp
build_usbsidpico: CPPFLAGS += -DBOARD='USBSIDPICO' $(CPPLIBUSBSIDPICO)
build_usbsidpico: LDLIBS += $(LDLIBUSBSIDPICO)
build_usbsidpico::
	$(info Starting $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(BUILD_DIR)/$(TARGET_EXEC_USBSIDPICO) $(SRCS) $(LDLIBS)

# ftdi build command ~ g++ -g3 $(pkg-config --cflags libftdi1) -o testje *.cpp $(pkg-config --libs libftdi1)
build_ft2232h: CPPFLAGS += -DBOARD='FTDI' $(CPPLIBFTDI)
build_ft2232h: LDLIBS += $(LDLIBFTDI)
build_ft2232h::
	$(info Starting $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(BUILD_DIR)/$(TARGET_EXEC_FTDI) $(SRCSFTDI) $(LDLIBS)

# raspberrypi build command ~ g++ *.cpp -D BOARD='RASPBERRYPI' -o sidberry -lwiringPi
build_raspberrypi: CPPFLAGS += -DBOARD='RASPBERRYPI' $(CPPLIBRPI)
build_raspberrypi: LDLIBS += $(LDLIBRPI)
build_raspberrypi::
	$(info Starting $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(BUILD_DIR)/$(TARGET_EXEC_RASPBERRYPI) $(SRCS) $(LDLIBS)

# arietta build command ~ g++ *.cpp -D BOARD='ARIETTAG25' -o sidberry -lwiringSam
build_arietta: CPPFLAGS += -DBOARD='ARIETTAG25' $(CPPLIBARIETTA)
build_arietta: LDLIBS += $(LDLIBARRIETTA)
build_arietta::
	$(info Starting $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(BUILD_DIR)/$(TARGET_EXEC_ARIETTAG25) $(SRCS) $(LDLIBS)

# custom build command ~ g++ *.cpp -D BOARD='CUSTOM' -o sidberry
build_custom: CPPFLAGS += -DBOARD='CUSTOM' $(CPPLIBCUSTOM)
build_custom: LDLIBS += $(LDLIBCUSTOM)
build_custom::
	$(info Starting $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(BUILD_DIR)/$(TARGET_EXEC_CUSTOM) $(SRCS) $(LDLIBS)
