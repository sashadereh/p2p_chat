#ifndef LOGGER_H
#define LOGGER_H

#include "utils.h"
#include <mutex>

class Logger
{
public:
    virtual ~Logger();
    static Logger& GetInstance();
    void Trace(const char*, ...);

private:
    // singleton class
    static unique_ptr<Logger> _instance;
    static once_flag _onceFlag;
    Mutex _writeMutex;

    Logger();
    Logger(const Logger& src);
    Logger& operator=(const Logger& rval);
};

#endif // LOGGER_H
