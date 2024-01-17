#include "log.h"
#include <memory>
//#include <sstream>
#include <thread>
//#include <iostream>

void otherfunc(std::shared_ptr<libQ::log> logobj)
{
  using namespace libQ;
  auto func2 = logobj->function("otherfunc");
  func2 << "Above FUNCTION only prints on DEBUG" << loglevel::NOTEDEBUG;
}

void test(std::shared_ptr<libQ::log> logobj)
{
  using namespace libQ;
  auto log = logobj->function("test");
  otherfunc(logobj);

  log << "NOTE - Allowed on DEFAULT" << loglevel::NOTE;
  log << "VALUE - Allowed on DEFAULT: " << 6 << loglevel::VALUE;
  log << "WARNING - Allowed on DEFAULT" << loglevel::WARNING;
  log << "ERROR - Allowed on DEFAULT" << loglevel::ERROR;
  log << "NOTEV - Allowed on V" << loglevel::NOTEV;
  log << "VALUEV - Allowed on V: " << 7 << loglevel::VALUEV;
  log << "NOTEVV - Allowed on VV" << loglevel::NOTEVV;
  log << "VALUEVV - Allowed on VV" << loglevel::VALUEVV;
  log << "NOTEDEBUG - Allowed on DEBUG" << loglevel::NOTEDEBUG;
  log << "VALUEDEBUG - Allowed on DEBUG" << loglevel::VALUEDEBUG;
}

class logtest
{
public:
  logtest(std::shared_ptr<libQ::log> _logobj)
  {
    logobj = _logobj;
    logobj->function("logtest");
  }
  void myfunc()
  {
    auto log = logobj->function("myfunc");
    log << "Printing from myfunc" << libQ::loglevel::NOTE;
  }
private:
  std::shared_ptr<libQ::log> logobj;
};

void threadfunc(libQ::log *ptr)
{
  std::shared_ptr<libQ::log> logobj;
  logobj.reset(ptr);
  auto log = logobj->function("threadfunc");
  log << "Printing from thread" << libQ::NOTE;
}

int main(int argc, char **argv)
{
  using namespace libQ;
  std::shared_ptr<libQ::log> logobj;
  logobj.reset(new libQ::log(vlevel::DEBUG));
  auto log = logobj->function("main");

  logtest myclass(logobj);
  myclass.myfunc();

  std::thread mythread(threadfunc,logobj->split("mythread"));

  logobj->changeVerbosity(vlevel::DEFAULT);
  log << "Sleeping for 750ms" << loglevel::NOTE;
  std::this_thread::sleep_for( std::chrono::milliseconds(750) );
  
  log << "---------------------------" << loglevel::NOTE;
  log << "Running function as DEFAULT" << loglevel::NOTE;
  logobj->changeVerbosity(vlevel::DEFAULT);
  test(logobj);
  log << "---------------------------" << loglevel::NOTE;
  log << "Running function as V" << loglevel::NOTE;
  logobj->changeVerbosity(vlevel::V);
  test(logobj);
  log << "---------------------------" << loglevel::NOTE;
  log << "Running function as VV" << loglevel::NOTE;
  logobj->changeVerbosity(vlevel::VV);
  test(logobj);
  log << "---------------------------" << loglevel::NOTE;
  log << "Running function as DEBUG" << loglevel::NOTE;
  logobj->changeVerbosity(vlevel::DEBUG);
  test(logobj);

  mythread.join();
}
