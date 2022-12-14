find_package(Threads REQUIRED)
add_executable(test main.cpp set_affinity.cpp)
target_compile_features(test PRIVATE cxx_std_20)
target_link_libraries(test PRIVATE Threads::Threads)

# Because coroutines are now allocated on the stack
# I'll increase the stack size for now
target_link_options(test PRIVATE -Wl,-stack_size -Wl,20000000) # 512MB (max allowed on macOS)

add_executable(mpsc_queue_test mpsc_queue_test.cpp)
target_compile_features(mpsc_queue_test PRIVATE cxx_std_20)
target_compile_options(mpsc_queue_test PRIVATE -fcoroutines-ts -fsanitize=address -fsanitize=undefined)
target_link_options(mpsc_queue_test PRIVATE -fsanitize=address -fsanitize=undefined)
target_link_libraries(mpsc_queue_test PRIVATE Threads::Threads)

add_executable(round_robin_manual_queue_test round_robin_manual_queue_test.cpp)
target_compile_features(round_robin_manual_queue_test PRIVATE cxx_std_20)
target_compile_options(round_robin_manual_queue_test PRIVATE -fcoroutines-ts -fsanitize=address -fsanitize=undefined)
target_link_options(round_robin_manual_queue_test PRIVATE -fsanitize=address -fsanitize=undefined)
target_link_libraries(round_robin_manual_queue_test PRIVATE Threads::Threads)

add_executable(round_robin_queue_test round_robin_queue_test.cpp)
target_compile_features(round_robin_queue_test PRIVATE cxx_std_20)
target_compile_options(round_robin_queue_test PRIVATE -fcoroutines-ts -fsanitize=address -fsanitize=undefined)
target_link_options(round_robin_queue_test PRIVATE -fsanitize=address -fsanitize=undefined)
target_link_libraries(round_robin_queue_test PRIVATE Threads::Threads)

add_executable(mpsc_list_test mpsc_list_test.cpp)
target_compile_features(mpsc_list_test PRIVATE cxx_std_20)
target_compile_options(mpsc_list_test PRIVATE -fcoroutines-ts -fsanitize=address) # -fsanitize=undefined)
target_link_options(mpsc_list_test PRIVATE -fsanitize=address) # -fsanitize=undefined)
target_link_libraries(mpsc_list_test PRIVATE Threads::Threads)
