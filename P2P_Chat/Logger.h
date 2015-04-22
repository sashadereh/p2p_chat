#ifndef LOGGER_H
#define LOGGER_H

#include "utils.h"
#include <mutex>
#include <queue>

#define LOGFILE_PATH "chat_client.log"

class SmartLoggerPtr;

class Logger
{
public:
    virtual ~Logger();
    static SmartLoggerPtr GetInstance();
    void Enable();
    void Disable();
    bool IsEnable() { return _enable; }
    template <typename T> void Trace(const T& t);
    template <typename First, typename... Rest> void Trace(const First& first, const Rest&... rest);
    queue<string>& GetQueue() { return _messageQueue; }
    friend SmartLoggerPtr;
private:
    // singleton class
    static unique_ptr<Logger> _instance;
    static once_flag _onceFlag;

    Mutex _loggerMutex;
    OutFile _logFile;
    stringstream _stringToInsert;
    queue<string> _messageQueue;
    bool _enable;
    bool _timePosted;

    Logger();
    Logger(const Logger& src);
    Logger& operator=(const Logger& rval);

    void InsertStringInFile(stringstream& strStream);
    void InsertTimeInString(stringstream& strStream);
    void MessageQueueHandler();
};

template <typename T>
void Logger::Trace(const T& t)
{
    if (!_enable)
        return;

    if (!_timePosted)
        InsertTimeInString(_stringToInsert);
    _stringToInsert << t << endl;

    string toInsert = _stringToInsert.str();
    _messageQueue.push(toInsert);

    _stringToInsert.str("");
    _timePosted = false;
}


template <typename First, typename... Rest>
void Logger::Trace(const First& first, const Rest&... rest)
{
    if (!_enable)
        return;

    if (!_timePosted)
    {
        InsertTimeInString(_stringToInsert);
        _timePosted = true;
    }        

    _stringToInsert << first;

    Trace(rest...);
}


class SmartLoggerPtr
{
public:
    SmartLoggerPtr(Logger& instance)
    {
        _logger = &instance;
        _logger->_loggerMutex.lock();
    }

    ~SmartLoggerPtr()
    {
        _logger->_loggerMutex.unlock();
    }

    Logger* operator ->()
    {
        return _logger;
    }
private:
    Logger* _logger;
};

#endif // LOGGER_H
