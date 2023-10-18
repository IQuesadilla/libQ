#ifndef CFIFO_H
#define CFIFO_H

#include "libQ.h"

// This class is NOT atomic - write and read should not occur simultaneously
// Write performs a copy, read does not. Might be easier to tho.
//   Unfortunately, this would mean dual-copy, which is unfortunate
// Unsure if reads and writes should block or not. This would solve many problems
class cfifo
{
public:
    cfifo(char *ptr, uint size);
    
    uint write(const char *data, uint length);
    uint read(char *data, uint length);

    bool putch(const char *byte);
    bool getch(char *byte);

    bool pop();

private:
    char *_buffer;
    uint _bufflen;

    uint _read_pos;
    uint _write_pos;
    bool _writeIsAhead;
};

#endif
