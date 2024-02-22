add_library( Tracer Tracer.cpp Debug.cpp )

target_compile_options( Tracer PUBLIC -g -Wall )
target_include_directories( Tracer PUBLIC "${PROJECT_SOURCE_DIR}/include" )
