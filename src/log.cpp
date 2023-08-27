#include "logcpp.h"
#include <iostream>
#include "colorcodes.h"

logcpp::logcpp(vlevel verbosity)
{
    tabs = 0;
    _verbosity = verbosity;
    start = std::chrono::system_clock::now();
}

logcpp::~logcpp()
{
    ;
}

void logcpp::flush(loglevel lev, const char *output)
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


    templogstream << '[' << std::string(12-ms.length(),'0') << ms << "] " << indent();

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

    output_lock.lock();
    std::cout << templogstream.str();
    output_lock.unlock();
}

lifetimelogcpp logcpp::function(const char *name)
{
    std::stringstream templogstream;
    templogstream << "(" << name << ")";
    flush(loglevel::FUNCTION,templogstream.str().c_str());
    ++tabs;
    return lifetimelogcpp(this);
}

void logcpp::endfunc()
{
    --tabs;
}

std::string logcpp::indent()
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

lifetimelogcpp & operator<<(lifetimelogcpp &buff, const char *output)
{
    buff.log(output);
    return buff;
}

lifetimelogcpp & operator<<(lifetimelogcpp &buff, const std::string output)
{
    buff.log(output.c_str());
    return buff;
}

lifetimelogcpp & operator<<(lifetimelogcpp &buff, const int output)
{
    buff.log(std::to_string(output).c_str());
    return buff;
}

lifetimelogcpp & operator<<(lifetimelogcpp &buff, logcpp::loglevel lev)
{
    buff.logobj->flush(lev,buff.logstream.str().c_str());

    buff.logstream.str("");
    buff.logstream.clear();

    return buff;
}

void lifetimelogcpp::log(const char *output)
{
    logstream << output;
}
