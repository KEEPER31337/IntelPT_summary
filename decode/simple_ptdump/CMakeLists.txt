include_directories(
  include
  ~/libipt/libipt/internal/include
)

set(PTDUMP_FILES
  simple_ptdump.c
  ~/libipt/libipt/src/pt_last_ip.c
  ~/libipt/libipt/src/pt_cpu.c
  ~/libipt/libipt/src/pt_time.c
)

add_executable(simple_ptdump
  ${PTDUMP_FILES}
)

target_link_libraries(simple_ptdump ipt)

