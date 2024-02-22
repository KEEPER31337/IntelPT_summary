cmake_minimum_required( VERSION 3.10 )
project( ProcessorTracerByIntelPT )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

if ( UNIX AND NOT APPLE )
    add_subdirectory( lib )

    add_executable( trace_test trace_test.cpp )
    target_compile_options( trace_test PUBLIC -g -Wall )
    target_link_libraries( trace_test Tracer )
    target_include_directories( trace_test
                                PUBLIC
                                "${PROJECT_SOURCE_DIR}/include" )
    set_target_properties( trace_test
                           PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY
                           ${CMAKE_BINARY_DIR}/bin )

else()
    message( FATAL_ERROR "This project is linux project" )
endif()
