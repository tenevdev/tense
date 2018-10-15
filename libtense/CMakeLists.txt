cmake_minimum_required(VERSION 3.10)
project(tense C)

set(CMAKE_C_STANDARD 11)

add_definitions(-D_FILE_OFFSET_BITS=64)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-pie -pg")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie -pg")
SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -no-pie -pg")

add_library(tense SHARED tense.c tense.h preload.c)
target_link_libraries(tense dl)

add_executable(health_check test/health_check_test.c tense.c tense.h)

set(THREAD_PREFER_PTHREAD_FLAG OM)
find_package(Threads REQUIRED)
add_executable(pthread_test test/pthread_test.c)
target_link_libraries(pthread_test Threads::Threads)
target_link_libraries(pthread_test tense)

add_executable(client_server test/client_server.c)
target_link_libraries(client_server Threads::Threads tense m)

add_executable(vtime_spin test/vtime_spin.c)
target_link_libraries(vtime_spin tense m)

add_executable(linux_time test/linux_time.c)
target_link_libraries(linux_time tense Threads::Threads)

add_executable(nop_calibration test/nop_calibration.c)

add_executable(io test/io.c)

add_executable(overhead test/overhead.c)
target_link_libraries(overhead tense Threads::Threads)

add_executable(tense_move test/tense_move.c)
target_link_libraries(tense_move tense)

add_executable(tense_move_thread test/tense_move_thread.c)
target_link_libraries(tense_move_thread tense Threads::Threads)

add_executable(tense_lock_race test/tense_lock_race.c)
target_link_libraries(tense_lock_race tense Threads::Threads)

add_executable(tense_lock_loop test/tense_lock_loop.c)
target_link_libraries(tense_lock_loop tense Threads::Threads)

add_executable(tense_sleep test/tense_sleep.c)
target_link_libraries(tense_sleep tense Threads::Threads)
