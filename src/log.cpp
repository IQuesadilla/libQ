#include "log.h"
#include <iostream>
#include "colorcodes.h"

namespace libQ {

log::log(vlevel verbosity)
{
    tabs = 0;
    _verbosity = verbosity;
    start = std::chrono::system_clock::now();
    output_lock.reset(new std::mutex());
}

log::log(log* copy)
{
    tabs = copy->tabs;
    _verbosity = copy->_verbosity;
    start = copy->start;
    output_lock = copy->output_lock;
}

log::~log()
{
    ;
}

void log::setPrefix(std::string prefix)
{
    _prefix = prefix;
}

void log::flush(loglevel lev, const char *output)
{
    switch (_verbosity)
    {
        case DEFAULT:
            if (lev == NOTEV ||
                lev == VALUEV) return;
        case V:
            if (lev == NOTEVV ||
                lev == VALUEVV) return;
        case VV:
            if (lev == FUNCTION ||
                lev == NOTEDEBUG ||
                lev == VALUEDEBUG) return;
        case DEBUG:
            // Allow all on debug
        break; 
    }

    std::stringstream templogstream;

    auto ms = std::to_string( std::chrono::duration_cast<std::chrono::milliseconds>
                    ( std::chrono::system_clock::now() - start).count() );


    templogstream << '[' << std::string(12-ms.length(),'0') << ms << "] " << _prefix << ' ' << indent();

    switch (lev)
    {
        case NOTE:
        case NOTEV:
        case NOTEVV:
        case NOTEDEBUG:
            templogstream << GREEN << "Note: ";
            break;
        case VALUE:
        case VALUEV:
        case VALUEVV:
        case VALUEDEBUG:
            templogstream << BLUE << "Value: ";
            break;
        case WARNING:
            templogstream << YELLOW << "Warning: ";
            break;
        case ERROR:
            templogstream << RED << "ERROR: ";
            break;
        case FUNCTION:
            templogstream << MAGENTA << "Function: ";
            break;
    }

    templogstream << output << RESET << std::endl;

    output_lock->lock();
    std::cout << templogstream.str();
    output_lock->unlock();
}

lifetimelog log::function(const char *name, bool disable_output)
{
    if ( !disable_output )
    {
        std::stringstream templogstream;
        templogstream << "(" << name << ")";
        flush(loglevel::FUNCTION,templogstream.str().c_str());
    }
    ++tabs;
    return lifetimelog(this);
}

void log::endfunc()
{
    --tabs;
}

std::string log::indent()
{   
    if (_verbosity == DEBUG)
    {
        std::stringstream os;
        for (int i = 0; i < tabs; ++i)
        {
            os << "  ";
        }
        return os.str();
    }
    else
        return "";
}

lifetimelog & operator<<(lifetimelog &buff, const char *output)
{
    buff._log(output);
    return buff;
}

lifetimelog & operator<<(lifetimelog &buff, const std::string output)
{
    buff._log(output.c_str());
    return buff;
}

lifetimelog & operator<<(lifetimelog &buff, const int output)
{
    buff._log(std::to_string(output).c_str());
    return buff;
}

lifetimelog & operator<<(lifetimelog &buff, libQ::loglevel lev)
{
    buff.logobj->flush(lev,buff.logstream.str().c_str());

    buff.logstream.str("");
    buff.logstream.clear();

    return buff;
}

void lifetimelog::_log(const char *output)
{
    logstream << output;
}

}