### Cmake minimum version
cmake_minimum_required(VERSION 3.17)

# the `pkg_check_modules` function is created with this call
find_package(PkgConfig REQUIRED)

### Cmake compiler standard versions
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

### Project magic sprinkles
set(PROJECT_NAME usbsidberry)

### Compiler flags and options
set(COMPILE_OPTS PRIVATE
  -g3
  -O0
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

### Source files to compile
set(SOURCEFILES
  ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/SidFile.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/mos6502/mos6502.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/driver/USBSID.cpp
)

project(${PROJECT_NAME} C CXX)

# these calls create special `PkgConfig::<MODULE>` variables
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
if (UNIX)
pkg_check_modules(UDEV REQUIRED IMPORTED_TARGET udev)
endif (UNIX)

### Libraries to link
if (UNIX)
set(TARGET_LL
  PkgConfig::LIBUSB
  PkgConfig::UDEV
)
endif (UNIX)
if (WIN32)
set(TARGET_LL
  PkgConfig::LIBUSB
)
endif (WIN32)

### Header folders to include
set(TARGET_INCLUDE_DIRS PRIVATE
  .
  src
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${CMAKE_CURRENT_LIST_DIR}/src/mos6502
  ${CMAKE_CURRENT_LIST_DIR}/src/driver
  /usr/local/lib
)

### Project type
project(${PROJECT_NAME} C CXX ASM)

### Compile time
add_executable(${PROJECT_NAME} ${SOURCEFILES})

target_include_directories(${PROJECT_NAME} ${TARGET_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${TARGET_LL})
target_sources(${PROJECT_NAME} PUBLIC ${SOURCEFILES})
target_compile_options(${PROJECT_NAME} ${COMPILE_OPTS})
