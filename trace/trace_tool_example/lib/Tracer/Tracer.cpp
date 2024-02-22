#include <cstring>
#include <fstream>
#include <iostream>
#include <linux/perf_event.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "Tracer.h"

void Tracer::InitTracer( void )
{
    setPerfEvent();
}

void Tracer::StartTrace( void )
{
    ioctl( _fd, PERF_EVENT_IOC_ENABLE, 0 );
}

void Tracer::StopTrace( void )
{
    ioctl( _fd, PERF_EVENT_IOC_DISABLE, 0 );
    SaveTraceData();
    SaveMapsData();
}

void Tracer::SaveTraceData( void )
{
    int total_trace_data_size = 0;
    int last_founded_not_null = 0;
    struct perf_event_mmap_page *header;
    header = ( struct perf_event_mmap_page * ) _base;
    for ( uint64_t i = 0; i < header->aux_size; ++ i )
    {
        if ( _aux[i] != 0 )
        {
            last_founded_not_null = i;
        }
    }
    total_trace_data_size += 2048;

    while ( total_trace_data_size < last_founded_not_null )
    {
        total_trace_data_size += 2048;
    }
    std::cout << "Total size of traced data : " << total_trace_data_size << std::endl;

    std::string filename = "traced_data.bin";
    FILE *traced_data_fp = fopen( filename.c_str(), "w" );
    if ( traced_data_fp == NULL )
    {
        std::cerr << filename << " open error : " << strerror( errno ) << std::endl;
        exit( 1 );
    }
    std::cout << "Save " << filename << " .. " << std::flush;
    fwrite( _aux, sizeof( char ), total_trace_data_size, traced_data_fp );
    std::cout << "done" << std::endl;
    fclose( traced_data_fp );
}

void Tracer::SaveMapsData( void )
{
    uint8_t *data_addr = _data + 16;
    int size = 0;
    struct mmap map;
    struct raw_file raw;
    while ( true )
    {
        memcpy( &map, data_addr, sizeof( struct mmap ) );
        size = sizeof( struct mmap );
        if ( map.tid != ( uint32_t )_target_tid )
        {
            break;
        }
        raw.path = ( const char * ) data_addr + size;
        size += raw.path.size();
        size += 8 - ( raw.path.size() ) % 8; // 8 bytes padding
        data_addr += size;
        if ( raw.path[0] != '/' )
        {
            continue;
        }
        raw.base = map.addr - map.pgoff;
        raw.end = map.addr + map.len;
        _raw_file_list.push_back( raw );
    }
    if ( _raw_file_list.empty() )
    {
        std::cout << "Maps data is empty" << std::endl;
    }
    else
    {
        _bin_path = _raw_file_list.at(0).path;
    }

    std::string filename = "maps_data";
    std::ofstream ofs_maps( filename, std::ios_base::binary );
    if ( !ofs_maps.is_open() )
    {
        std::cerr << "maps_data open error" << std::endl;
        exit( 1 );
    }
    ofs_maps << _raw_file_list.size() << std::endl;
    for ( auto it : _raw_file_list )
    {
        ofs_maps << it.path << std::endl;
        ofs_maps << it.base << std::endl;
        ofs_maps << it.end << std::endl;
    }
    ofs_maps.close();
}

void Tracer::setPerfEvent( void )
{
    // init attr
    memset( &_attr, 0, sizeof( _attr ) );
    _attr.config         = setPtConfig();
    _attr.size           = sizeof( _attr );
    _attr.type           = getIntelPtType();
    _attr.mmap           = 1;
    _attr.exclude_kernel = 1;
    _attr.task           = 1;

    // If use binary, then set enable_on_exec 1.
    _attr.enable_on_exec = 1;
    _attr.disabled       = 1;

    // call perf_event_open syscall
    _fd = syscall( SYS_perf_event_open, &_attr, _target_tid, -1, -1, 0 );
    if ( _fd == -1 ) {
        std::cerr << "perf_event_open is failed : " << _target_tid << std::endl;
        return ;
    }

    // setting AUX and DATA area
    long PAGESIZE = sysconf( _SC_PAGESIZE );
    struct perf_event_mmap_page *header;

    _base = ( uint8_t * ) mmap( NULL, ( 1 + 128 ) * PAGESIZE, PROT_WRITE, MAP_SHARED, _fd, 0 );
    if ( _base == MAP_FAILED ) {
        std::cerr << "base : mmap failed : " << errno << std::endl;
        exit( -1 );
    }

    header = ( struct perf_event_mmap_page * )_base;
    _data = ( uint8_t * )_base + header->data_offset;

    header->aux_offset = header->data_offset + header->data_size;
    header->aux_size   = 128 * 128 * PAGESIZE * 16;

    _aux = ( uint8_t * ) mmap( NULL, header->aux_size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, header->aux_offset );
    if ( _aux == MAP_FAILED ) {
        std::cerr << "aux : mmap failed : " << errno << std::endl;
        return ;
    }
    ioctl( _fd, PERF_EVENT_IOC_RESET, 0 );
}

uint32_t Tracer::setPtConfig( void )
{
    uint32_t config = 0;
    config |= 0b1 << getPtConfigBit( "pt" ); 
    // config |= 0b1 << getPtConfigBit( "tsc" );
    config |= 0b1 << getPtConfigBit( "noretcomp" );
    config |= 0b1 << getPtConfigBit( "branch" );
    // config |= 0b11 << getPtConfigBit( "psb_period" );
    // config |= 0b1 << getPtConfigBit( "mtc" );
    // config |= 6 << getPtConfigBit( "mtc_period" );
    // config |= 0b1 << getPtConfigBit( "cyc" );
    // config |= 0 << getPtConfigBit( "cyc_thresh" );

    return config;
}

int32_t Tracer::getPtConfigBit( std::string term )
{   
    std::string target_file = "/sys/bus/event_source/devices/intel_pt/format/" + term;
    std::ifstream ifs( target_file );
    if ( !ifs.is_open() ) {
        std::cerr << target_file + " open failed" << std::endl;
        exit( -1 );
    }
    std::string config;
    ifs >> config;
    ifs.close();

    std::string bit;
    if ( config.find( "-" ) != std::string::npos ) {
        bit = config.substr( config.find(":") + 1, config.find( "-" ) - config.find( ":" ) - 1 );
    }
    else {
        bit = config.substr( config.find(":") + 1, 2 );
    }

    return std::stoi( bit );
}

uint32_t Tracer::getIntelPtType( void )
{
    uint32_t type;
    // std::ifstream ifs( "/sys/devices/intel_pt/type" );
    std::ifstream ifs( "/sys/bus/event_source/devices/intel_pt/type" );
    if ( !ifs.is_open() ) {
        std::cerr << "/sys/bus/event_source/devices/intel_pt/type open failed" << std::endl;
        exit( -1 );
    }
    ifs >> type;
    ifs.close();
    return type;
}
