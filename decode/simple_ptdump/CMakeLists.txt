cmake_minimum_required(VERSION 3.11)

add_executable(simple_ptdump simple_ptdump.c)

target_compile_options(simple_ptdump PUBLIC -g -Wall)

target_link_libraries(simple_ptdump ipt)



