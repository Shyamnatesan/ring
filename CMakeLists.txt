cmake_minimum_required(VERSION 3.16)

project(ring LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

add_library(ring INTERFACE)
target_include_directories(ring INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(ring INTERFACE cxx_std_23)

add_executable(ring_demo src/main.cpp)
target_link_libraries(ring_demo PRIVATE ring Threads::Threads)


if(MSVC)
  target_compile_options(ring_demo PRIVATE /W4)
else()
  target_compile_options(ring_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()
