cmake_minimum_required(VERSION 3.14)
project(SchedulerProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The SchedulerModule is the module that is gonna be exposed to the google testing binaries.
add_library(
    SchedulerModule
    SchedulerModule/Private/Scheduler.cpp
    SchedulerModule/Public/Scheduler.hpp
    SchedulerModule/Private/SchedulerStats.cpp
    SchedulerModule/Public/SchedulerStats.hpp
    SchedulerModule/Public/Task.hpp
)

target_include_directories(SchedulerModule PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/SchedulerModule/Public
)

# Add GoogleTest unit tests and main executable
add_subdirectory(third_party/googletest)

enable_testing()

add_executable(SchedulerTests
    tests/test_scheduler.cpp
)

target_include_directories(SchedulerTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/SchedulerModule/Public
)

target_link_libraries(SchedulerTests
    SchedulerModule
    gtest
    gtest_main
)
