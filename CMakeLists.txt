cmake_minimum_required(VERSION 3.20)
project(job_system)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")

find_package(Threads REQUIRED)
add_executable(test main.cpp)
target_compile_features(test PRIVATE cxx_std_20)
target_compile_options(test PRIVATE -fcoroutines-ts)
target_link_libraries(test PRIVATE Threads::Threads)
