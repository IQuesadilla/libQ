#ifndef log_H
#define log_H
#pragma once

#include <sstream>
#include <mutex>
#include <memory>
#include <functional>

namespace libQ {
    
class lifetimelog;

//class loglevel { public:
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
//};

enum vlevel
{
    DEFAULT,
    V,
    VV,
    DEBUG
};

enum _PrintFName
{
  PRINTFUNCTION = 0,
  DELAYPRINTFUNCTION,
  NEVERPRINTFUNCTION
};

class log
{
public:
  log();
  log(vlevel verbosity);
  log(vlevel verbosity, std::function<void(const std::string)> PrintCallback);
  log(log* copy);
  log &operator=(const log& copy);
  ~log();

  void setPrefix(const char *prefix);
  void setClass(const char *classname);

  void printall(const char *func, const char *body, loglevel lev);
  lifetimelog function(const char *name, _PrintFName doFName = PRINTFUNCTION);
  lifetimelog operator()(const char *name, _PrintFName doFName = PRINTFUNCTION);
  log *split(const char *tname);

  class printer
  {
  public:
    printer();
    void setColors(bool enable);
    void setVerbosity(vlevel v);
    void setCallback(std::function<void(const std::string)> PrintCallback);
    template<typename T, typename F>
    void setCallback(T &obj, F PrintCallback)
    {
      setCallback([&obj,PrintCallback](const std::string& s){ (obj.*PrintCallback)(s);});
    }
  private:
    std::mutex output_lock;
    std::function<void(const std::string)> _PrintCallback;
    vlevel _verbosity;
    bool doColors;
    friend class log;
  };
  printer &operator[](size_t index);

private:
    void endfunc();

    std::string indent(vlevel verbosity); 

    static void DefaultPrint(const std::string);
    
    struct _internals
    {
      struct _globals
      {
        std::unordered_map<int,printer> printers;
        std::chrono::system_clock::time_point start;
      };
      std::shared_ptr<_globals> globals;
      int tabs;
      std::string _threadprefix;
    };

    log(std::shared_ptr<_internals> copy);
    log(std::shared_ptr<_internals::_globals> copy);
    std::shared_ptr<_internals> internals;
    std::string _prefix, _classname;

  friend class lifetimelog;
};

class lifetimelog
{
public:
    lifetimelog(log *parent, const char *inital = nullptr);
    ~lifetimelog() { logobj->endfunc(); }

    friend lifetimelog & operator<<(lifetimelog &buff, const char *output);
    friend lifetimelog & operator<<(lifetimelog &buff, const std::string output);
    friend lifetimelog & operator<<(lifetimelog &buff, const int output);
    friend lifetimelog & operator<<(lifetimelog &buff, const long output);
    friend lifetimelog & operator<<(lifetimelog &buff, const float output);
    friend void operator<<(lifetimelog &buff, const libQ::loglevel lev);

private:
    void _log(const char *output);

    std::stringstream logstream;
    std::string FuncBuff;
    libQ::log *logobj;
};

}

#endif
