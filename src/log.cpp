#include "log.h"
#include <iostream>
#include "colorcodes.h"

namespace libQ {

log::log() {
  internals = nullptr;
}

log::log(vlevel verbosity)
{
  internals.reset(new _internals);
  internals->tabs = 0;
  internals->globals.reset(new _internals::_globals);

  //internals->globals->printers.emplace(0,printer());
  internals->globals->printers[0].setVerbosity(verbosity);
  internals->globals->start = std::chrono::system_clock::now();
  internals->globals->printers[0]._PrintCallback = DefaultPrint; 
}

log::log(vlevel verbosity, std::function<void(const std::string)> PrintCallback)
{
  internals.reset(new _internals);
  internals->tabs = 0;
  internals->globals.reset(new _internals::_globals);
  //internals->globals->printers.emplace(0,printer());
  internals->globals->printers[0].setVerbosity(verbosity);
  internals->globals->start = std::chrono::system_clock::now();
  internals->globals->printers[0]._PrintCallback = PrintCallback;
}

log::log(log* copy)
{
  internals.reset(new _internals);
  *internals = *(copy->internals);
  //  tabs = copy->tabs;
  //  _verbosity = copy->_verbosity;
  //  start = copy->start;
  //  output_lock = copy->output_lock;
  //_PrintCallback = copy->_PrintCallback;
}

log &log::operator=(const log& copy)
{
  internals = copy.internals;
  return *this;
}

log::log(std::shared_ptr<_internals> copy)
{
  internals = copy;
}

log::log(std::shared_ptr<_internals::_globals> copy)
{
  internals.reset(new _internals);
  internals->tabs = 0;
  internals->globals = copy;
}

log::~log()
{
    ;
}

void log::setPrefix(const char *prefix)
{
  _prefix = std::string(prefix) + ' ';
}

void log::setClass(const char *classname)
{
  _classname = std::string(classname) + "::";
}

void log::printall(const char *func, const char *body, loglevel lev)
{
  //bool DoFuncIndent = (func && *func) || lev
  for (auto &x : internals->globals->printers)
  {
    auto verbosity = x.second._verbosity;
    bool failed = false;
    switch (verbosity)
    {
      // Always allows ERROR, WARNING, NOTE, VALUE
      case DEFAULT:
      if (lev == NOTEV ||
        lev == VALUEV) failed = true;
      case V:
        if (lev == NOTEVV ||
          lev == VALUEVV) failed = true;
      case VV:
        if (lev == FUNCTION ||
          lev == NOTEDEBUG ||
          lev == VALUEDEBUG) failed = true;
      case DEBUG:
        // Allow all on debug
      break; 
    }
    if (failed) continue;

    std::stringstream templogstream;
    //const char *arr[] = {func,body};
    //for (int i = 0; i < sizeof(arr)/sizeof(*arr); ++i)
    auto PrintLambda = [this,&x,&templogstream](const char *output, loglevel lev)
    { // TODO: Some of this can be optimised by only calculating it once
//      if (!output || !*output) return;
      //const char *output = arr[i]; 
      auto ms = std::to_string( std::chrono::duration_cast<std::chrono::milliseconds>
                    ( std::chrono::system_clock::now() - internals->globals->start).count() );

      if (lev == FUNCTION) --internals->tabs;
      int wslen = 6-ms.length(); wslen = (wslen > 0) ? wslen : 0;
      templogstream << '[' << std::string(wslen,' ') << ms << "]"
        << internals->_threadprefix << ' ' << indent(x.second._verbosity) << _prefix;
      if (lev == FUNCTION) ++internals->tabs;

      //if (!_prefix.empty()) templogstream << _prefix << ' ';

      //std::string prefix = _prefix + ' ';
      switch (lev)
      {
        case NOTE:
        case NOTEV:
        case NOTEVV:
        case NOTEDEBUG:
          if (x.second.doColors) templogstream << GREEN;
          templogstream << "Note: ";
          break;
        case VALUE:
        case VALUEV:
        case VALUEVV:
        case VALUEDEBUG:
          if (x.second.doColors) templogstream << BLUE;
          templogstream << "Value: ";
          break;
        case WARNING:
          if (x.second.doColors) templogstream << YELLOW;
          templogstream << "Warning: ";
          break;
        case ERROR:
          if (x.second.doColors) templogstream << RED;
          templogstream << "ERROR: ";
          break;
        case FUNCTION:
          if (x.second.doColors) templogstream << MAGENTA;
          templogstream << "Function: ";
          //++internals->tabs;
          break;
      } 

      templogstream << output;
      if (x.second.doColors) templogstream << RESET;
      templogstream << '\n';// << std::endl
    };

    if (func && *func)
      PrintLambda(func,FUNCTION);
    PrintLambda(body,lev);

    x.second.output_lock.lock();
    x.second._PrintCallback(templogstream.str().c_str());
    x.second.output_lock.unlock();
  }
}
/*
void log::printall(const char *uform, const char *form, loglevel lev)
{
  for (auto &x : internals->globals->printers)
  {
    std::string str = format(lev,uform,x.second._verbosity) + format(lev,form,x.second._verbosity);
    flush(str.c_str());
  }
}*/

lifetimelog log::function(const char *name, _PrintFName doFName)
{
  if (!internals)
    return lifetimelog(nullptr);


  std::stringstream templogstream;
  templogstream << "(" << _classname << name << ")";
  //std::string fstr = format(loglevel::FUNCTION,templogstream.str().c_str());
  std::string buff = templogstream.str();
  ++internals->tabs;
  switch ( doFName )
  {
  default:
  case PRINTFUNCTION:
    printall(nullptr,buff.c_str(),FUNCTION);
    //++internals->tabs;
    //for (auto &x : internals->globals->printers)
    //flush(format(FUNCTION,buff.c_str(),x.second._verbosity).c_str());
    return lifetimelog(this);
  case DELAYPRINTFUNCTION:
    return lifetimelog(this,buff.c_str());//format(FUNCTION,buff.c_str(),DEBUG).c_str());
  case NEVERPRINTFUNCTION:
    //++internals->tabs;
    return lifetimelog(this);
  }
}

lifetimelog log::operator()(const char *name, _PrintFName doFName)
{
  return function(name,doFName);
}

log *log::split(const char *tname)
{
  log *r = new log(internals->globals);
  r->internals->_threadprefix = "<" + std::string(tname) + ">";
  return r;
}

log::printer::printer()
{
  setColors(true);
}

void log::printer::setColors(bool enable)
{
  doColors = enable;
}

void log::printer::setVerbosity(vlevel verbosity)
{
  _verbosity = verbosity;
}

void log::printer::setCallback(std::function<void(const std::string)> PrintCallback)
{
  _PrintCallback = PrintCallback;
}

log::printer &log::operator[](size_t index)
{
  return internals->globals->printers[index];
}

void log::endfunc()
{
  --internals->tabs;
}

std::string log::indent(vlevel _verbosity)
{   
    if (_verbosity == DEBUG)
    {
        std::stringstream os;
        for (int i = 0; i < internals->tabs; ++i)
        {
            os << "  ";
        }
        return os.str();
    }
    else
        return "";
}

void log::DefaultPrint(const std::string output)
{
  std::cout << output << std::flush;
}

lifetimelog::lifetimelog(log *parent, const char *initial)
{
  logobj = parent;
  FuncBuff = "";
  if (initial) FuncBuff = std::string(initial);
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

lifetimelog & operator<<(lifetimelog &buff, const long output)
{
    buff._log(std::to_string(output).c_str());
    return buff;
}

lifetimelog & operator<<(lifetimelog &buff, const float output)
{
    buff._log(std::to_string(output).c_str());
    return buff;
}

void operator<<(lifetimelog &buff, libQ::loglevel lev)
{
  //buff.currentlog += buff.logobj->format(lev,buff.logstream.str().c_str());
  //buff.logobj->flush(buff.currentlog.c_str());

  if (buff.logobj)
    buff.logobj->printall(buff.FuncBuff.c_str(),buff.logstream.c_str(),lev);

  buff.logstream = "";
  buff.logstream.clear();
  buff.FuncBuff = "";
}

void lifetimelog::_log(const char *output)
{
  if (logobj)
    logstream.append(output);
}

}
