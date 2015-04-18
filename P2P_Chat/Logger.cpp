#include "Logger.h"

unique_ptr<Logger> Logger::_instance;
once_flag Logger::_onceFlag;

Logger::Logger() : _timePosted(false)
{
    _logFile.open(LOGFILE_PATH, ios_base::app);
    if (!_logFile.is_open())
        exit(1);
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
    LoggerThread.reset(new Thread(boost::bind(&Logger::MessageQueueHandler, this)));
}

void Logger::Disable()
{
    _enable = false;
    if (LoggerThread.get())
        LoggerThread->join();
    LoggerThread.reset();
}


SmartLoggerPtr Logger::GetInstance()
{
    call_once(_onceFlag, [] {
        _instance.reset(new Logger);
    }
    );
    if (_instance.get())
        return SmartLoggerPtr(*_instance.get());
    else
        throw logic_error("Logger singleton was deleted!");
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


void Logger::MessageQueueHandler()
{
    while (!DoShutdown && _enable)
    {
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        {
            ScopedLock lk(_loggerMutex);
            while (!_messageQueue.empty())
            {
                _logFile << _messageQueue.front();
                _messageQueue.pop();
            }
        }
    }

}
