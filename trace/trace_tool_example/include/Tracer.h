#ifndef TRACE_H_
#define TRACE_H_

#include <cstddef>
#include <cstdint>
#include <linux/perf_event.h>
#include <string>
#include <sys/types.h>
#include <vector>

struct raw_file
{
    std::string path;
    uint64_t    base;
    uint64_t    end;
};

struct mmap
{
    struct perf_event_header header;
    uint32_t pid;
    uint32_t tid;
    uint64_t addr;
    uint64_t len;
    uint64_t pgoff;
};

class Tracer
{
public:
    Tracer( pid_t tid )
    {
        _target_tid = tid;
        InitTracer();
    }

    void        InitTracer              ( void );
    void        SaveMapsData            ( void );
    void        SaveTraceData           ( void );
    void        StartTrace              ( void );
    void        StopTrace               ( void );

    /* For debug */
    pid_t       GetTargetTid_debug      ( void );
    void        CheckTraceResult_debug  ( void );


private:
    uint32_t    getIntelPtType          ( void );
    int32_t     getPtConfigBit          ( std::string term );
    uint32_t    setPtConfig             ( void );
    void        setPerfEvent            ( void );

    

private:
    struct perf_event_attr  _attr;
    uint8_t *               _aux;
    uint8_t *               _base;
    std::string             _bin_path;    
    uint8_t *               _data;
    int                     _fd;
    std::vector<raw_file>   _raw_file_list;
    pid_t                   _target_tid;
};
#endif /* TRACE_H_ */
