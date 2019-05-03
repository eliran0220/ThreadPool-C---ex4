cmake_minimum_required(VERSION 3.12)
project(ex4 C)
find_package(Threads)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

add_executable(ex4 main.c osqueue.c)