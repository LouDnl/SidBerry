### Cmake minimum version
cmake_minimum_required(VERSION 3.17)

### Cmake compiler standard versions
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

### Project magic sprinkles
set(PROJECT_NAME usbsidberry)

### Compiler flags and options
set(COMPILE_OPTS PRIVATE
  -g3
  -O0
  # -Og
  # -Wall
  # -Wunused
  # -Werror
  -fno-omit-frame-pointer
  -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
  -Wno-unused-function # extra check to verify no unused lingering code is present
  -Wno-maybe-uninitialized
  -save-temps
  -fverbose-asm
)

# Windows additionals
if (WIN32)
set(WIN32_SRC
  ${CMAKE_CURRENT_LIST_DIR}/src/termiwin/termiWin.c
)
set(WIN32_INC
  ${CMAKE_CURRENT_LIST_DIR}/src/termiwin
)
endif (WIN32)


# Run the project command
project(${PROJECT_NAME} C CXX ASM)

# the `pkg_check_modules` function is created with this call
find_package(PkgConfig REQUIRED)

# these calls create special `PkgConfig::<MODULE>` variables
pkg_check_modules(libusb REQUIRED IMPORTED_TARGET libusb-1.0)

### Libraries to link
if (UNIX)
pkg_check_modules(udev REQUIRED IMPORTED_TARGET libudev)
pkg_check_modules(asound REQUIRED IMPORTED_TARGET alsa)
# pkg_check_modules(jack REQUIRED IMPORTED_TARGET jack)
# pkg_check_modules(pthread REQUIRED IMPORTED_TARGET libpthread)
set(TARGET_LL
  PkgConfig::libusb
  PkgConfig::udev
  PkgConfig::asound
  # PkgConfig::pthread
)
endif (UNIX)
if (WIN32)
#pkg_check_modules(asound REQUIRED IMPORTED_TARGET libwinmm)
set(TARGET_LL
  PkgConfig::libusb
  #PkgConfig::libwinmm
  -lwinmm
)
endif (WIN32)

### Source files to compile
set(SOURCEFILES
  ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/SidFile.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/mos6502/mos6502.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/driver/src/USBSID.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/midi/RtMidi.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/midi/asid.cpp
  ${WIN32_SRC}
)
### Header folders to include
set(TARGET_INCLUDE_DIRS PRIVATE
  .
  src
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${CMAKE_CURRENT_LIST_DIR}/src/mos6502
  ${CMAKE_CURRENT_LIST_DIR}/src/driver/src
  ${CMAKE_CURRENT_LIST_DIR}/src/midi
  ${WIN32_INC}
  /usr/local/lib
  /usr/local/include
  /usr/lib
  /usr/include
)

### Compile time
add_executable(${PROJECT_NAME} ${SOURCEFILES})

if (UNIX)
target_compile_definitions(${PROJECT_NAME} PRIVATE UNIX_COMPILE)
# target_compile_definitions(${PROJECT_NAME} PRIVATE __LINUX_ALSA__)
# target_compile_definitions(${PROJECT_NAME} PRIVATE __RTMIDI_DEBUG__)
endif (UNIX)

if (WIN32)
target_compile_definitions(${PROJECT_NAME} PRIVATE TERMIWIN_DONOTREDEFINE=1)
target_compile_definitions(${PROJECT_NAME} PRIVATE WINDOWS_COMPILE)
#target_compile_definitions(${PROJECT_NAME} PRIVATE __WINDOWS_MM__)
endif (WIN32)

target_include_directories(${PROJECT_NAME} ${TARGET_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${TARGET_LL})
target_sources(${PROJECT_NAME} PUBLIC ${SOURCEFILES})
target_compile_options(${PROJECT_NAME} ${COMPILE_OPTS})
