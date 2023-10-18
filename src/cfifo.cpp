#include "cfifo.h"

cfifo::cfifo(char *ptr, uint length)
{
    _buffer = ptr;
    _bufflen = length;

    _write_pos = 0;
    _read_pos = 0;

    _writeIsAhead = false;
}

uint cfifo::write(const char *data, uint length)
{
    return 0;
}

uint cfifo::read(char *data, uint length)
{
    return 0;
}

bool cfifo::putch(const char *byte)
{
    if (_write_pos == _bufflen)
    {
        _write_pos = 0;
        _writeIsAhead = true;
    }

    if (_write_pos == _read_pos && _writeIsAhead)
    {
        return false;
    }

    _buffer[_write_pos] = *byte;
    ++_write_pos;
    return true;
}

bool cfifo::getch(char *byte)
{
    if (_read_pos == _bufflen)
    {
        _read_pos = 0;
        _writeIsAhead = false;
    }

    if (_read_pos == _write_pos && !_writeIsAhead)
    {
        return false;
    }

    *byte = _buffer[_read_pos];
    ++_read_pos;
    return true;
}