cmake_minimum_required(VERSION 2.8.12)
project(lab2)

file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)

add_executable(lab2 ${SOURCES})

target_compile_options(lab2 PUBLIC -std=c++11)

find_package(Boost COMPONENTS thread system REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

target_link_libraries(lab2 PUBLIC ${Boost_LIBRARIES})
