#include "../logcpp.h"
#include <memory>
#include <sstream>
#include <thread>

void otherfunc(std::shared_ptr<logcpp> logobj)
{
    auto func2 = logobj->function("newfunc");
    func2 << "Here inside" << logcpp::loglevel::NOTE;
}

int main(int argc, char **argv)
{
    std::shared_ptr<logcpp> logobj;
    logobj.reset(new logcpp(logcpp::vlevel::DEFAULT));

    auto log = logobj->function("main");
    log << "Here outer" << logcpp::loglevel::NOTE;

    otherfunc(logobj);

    std::this_thread::sleep_for( std::chrono::milliseconds(750) );
    
    log << "The number six: " << 6 << logcpp::loglevel::VALUE;
    log << "This is a warning" << logcpp::loglevel::WARNING;
    log << "Here is an error" << logcpp::loglevel::ERROR;
    log << "This is a verbose note" << logcpp::loglevel::NOTEV;
    log << "This is a doubly verbose note" << logcpp::loglevel::NOTEVV;
    log << "This is a debug note" << logcpp::loglevel::NOTEDEBUG;
}