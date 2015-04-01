#include "Logger.h"

unique_ptr<Logger> Logger::_instance;
once_flag Logger::_onceFlag;

Logger::Logger()
{

}

Logger::~Logger()
{

}

Logger& Logger::GetInstance()
{
    call_once(_onceFlag, [] {
        _instance.reset(new Logger);
    }
    );
    return *_instance.get();
}

void Logger::Trace(const char*, ...)
{

}
