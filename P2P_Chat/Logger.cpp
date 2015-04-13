#include "Logger.h"

unique_ptr<Logger> Logger::_instance;
once_flag Logger::_onceFlag;

Logger::Logger() : _timePosted(false)
{
    cout << "In Logger ctor" << endl;
    _logFile.open(LOGFILE_PATH, ios_base::app);
    if (!_logFile.is_open())
        exit(1);
    Enable();
    _addToQueueMutex.initialize();
}


Logger::~Logger()
{
    cout << "In Logger dtor" << endl;
    _logFile.close();
    _addToQueueMutex.destroy();
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


void Logger::MessageQueueHandler()
{
    boost::this_thread::sleep(boost::posix_time::seconds(2));
    while (!DoShutdown)
    {
        while (!_messageQueue.empty())
        {
            cout << _messageQueue.front();
            _messageQueue.pop();
        }
    }
}
