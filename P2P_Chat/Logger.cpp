#include "Logger.h"

unique_ptr<Logger> Logger::_instance;
once_flag Logger::_onceFlag;

Logger::Logger() : _timePosted(false)
{
    _loggerMutex.initialize();
    Enable();
}


Logger::~Logger()
{
    Disable();
    _logFile.close();
    _loggerMutex.destroy();
}

void Logger::Enable()
{
    _enable = true;
    ThreadsMap.insert(pair<cc_string, auto_ptr<Thread>>(LOG_THREAD,
        auto_ptr<Thread>(new Thread(boost::bind(&Logger::MessageQueueHandler, this)))));
}

void Logger::Disable()
{
    _enable = false;

    if (ThreadsMap[LOG_THREAD].get())
        ThreadsMap[LOG_THREAD]->join();

    ThreadsMap[LOG_THREAD].reset();
    ThreadsMap.erase(LOG_THREAD);
}


SmartLoggerPtr Logger::GetInstance()
{
    call_once(_onceFlag, [] {
        _instance.reset(new Logger);
    }
    );
    return SmartLoggerPtr(*_instance.get());
}


void Logger::InsertTimeInString(stringstream& strStream)
{
    time_t rawTime;
    struct tm* timeinfo;
    char buffer[50];

    time(&rawTime);
    timeinfo = localtime(&rawTime);

    strftime(buffer, 50, "[%Y-%m-%d %H:%M:%S]", timeinfo);
    strStream << buffer;
}

// Thread function: LOG_THREAD

void Logger::MessageQueueHandler()
{
    while (!DoShutdown && _enable)
    {
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        {
            ScopedLock lk(_loggerMutex);
            _logFile.open(LOGFILE_PATH, ios_base::app);
            while (!_messageQueue.empty())
            {
                _logFile << _messageQueue.front();
                _messageQueue.pop();
            }
            _logFile.close();
        }
    }

}
