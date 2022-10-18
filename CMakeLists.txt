cmake_minimum_required(VERSION 3.20)
project(job_system)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_subdirectory(third_party)
add_subdirectory(src)
