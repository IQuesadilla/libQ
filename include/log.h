#ifndef log_H
#define log_H
#pragma once

#include <sstream>
#include <mutex>
#include <memory>

namespace libQ {
    
class lifetimelog;

enum loglevel
{
    ERROR,
    WARNING,
    NOTE,
    NOTEV,
    NOTEVV,
    NOTEDEBUG,
    VALUE,
    VALUEV,
    VALUEVV,
    VALUEDEBUG,
    FUNCTION
};

enum vlevel
{
    DEFAULT,
    V,
    VV,
    DEBUG
};

class log
{
public:
    log(vlevel verbosity);
    log(log* copy);
    ~log();

    void setPrefix(std::string prefix);

    void flush(loglevel lev, const char *output);
    lifetimelog function(const char *name, bool disable_output = false);
    void endfunc();

private:
    std::string indent();

    vlevel _verbosity;
    std::shared_ptr<std::mutex> output_lock;
    std::chrono::system_clock::time_point start;
    int tabs;
    std::string _prefix;
};

class lifetimelog
{
public:
    lifetimelog(log *log) { logobj = log; }
    ~lifetimelog() { logobj->endfunc(); }

    friend lifetimelog & operator<<(lifetimelog &buff, const char *output);
    friend lifetimelog & operator<<(lifetimelog &buff, const std::string output);
    friend lifetimelog & operator<<(lifetimelog &buff, const int output);
    friend lifetimelog & operator<<(lifetimelog &buff, const libQ::loglevel lev);

private:
    void _log(const char *output);

    std::stringstream logstream;
    libQ::log *logobj;
};

}

#endif