#include <iostream>

#include "Tracer.h"

pid_t Tracer::GetTargetTid_debug( void )
{
    return _target_tid;
}

void Tracer::CheckTraceResult_debug( void )
{
    for ( int i = 0; i < 32; ++ i ) {
        std::cout << std::hex << ( int )_aux[i] << " " << std::dec;
    }
    std::cout << std::endl;
}
