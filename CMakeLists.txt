cmake_minimum_required(VERSION 3.16)
project(SCR_CRON_V2)

find_package(Threads)

set(CMAKE_CXX_STANDARD 11)

add_executable(SCR_CRON_V2 log.c cron.cpp)

target_link_libraries(SCR_CRON_V2 pthread rt)