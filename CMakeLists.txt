# Note: This is not the primary build system for the project.
# The authoritative build and test configuration is defined in the Makefile.
# This CMake file exists solely to support IDE integration and assist
# contributors in importing the project into development environments.

cmake_minimum_required(VERSION 3.14)
project(jurand CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(jurand src/jurand.cpp)

enable_testing()
add_executable(jurand_test src/jurand_test.cpp)
add_test(NAME jurand_test COMMAND jurand_test)
