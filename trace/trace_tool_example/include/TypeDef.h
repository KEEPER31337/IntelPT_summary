#ifndef TYPEDEF_H_
#define TYPEDEF_H_

#include <linux/perf_event.h>
#include <cstdint> 
#include <string>
#include <vector>

struct raw_file {
    std::string path;
    uint64_t    base;
    uint64_t    end;
};

struct mmap {
    struct perf_event_header header;
    uint32_t    pid;
    uint32_t    tid;
    uint64_t    addr;
    uint64_t    len;
    uint64_t    pgoff;
};

#endif /* TYPEDEF_H_ */
