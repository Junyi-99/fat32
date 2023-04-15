cmake_minimum_required(VERSION 3.21)
project(fat)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)
option(ENABLE_ASAN "Enable address sanitizer" OFF)

add_compile_options(-O0 -g -Wall -Wno-unused-result -Wno-unused-but-set-variable -Wno-unused-variable)

if(ENABLE_ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address -fno-omit-frame-pointer)
endif()

set(SOURCE_FILES main.cpp myfat.cpp myfat.h)

add_executable(fat ${SOURCE_FILES})
