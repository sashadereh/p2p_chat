#include "ChatClient.h"
#include "Logger.h"
#include <boost/lexical_cast.hpp>

int main(int argc, char ** argv)
{
    ChatClient* chatInstance = &ChatClient::GetInstance();
//     Logger::GetInstance()->Trace("hi");
//     Logger::GetInstance()->Trace("hi", 2, "yes", 6, 7);
//     Logger::GetInstance()->Trace("again");
//     Logger::GetInstance()->Trace("again", 2, "yes");
    cout << "Start test" << endl;
    Logger::GetInstance().GetLogger();
    cout << "End test" << endl;
  
#if 0
    if (argc > 1)
    {
        try
        {
            PORT = boost::lexical_cast<int>(argv[1]);
        }
        catch (const boost::bad_lexical_cast& err)
        {
            cerr << err.what();
        }
    }

    ChatClient* chatInstance = &ChatClient::GetInstance();

//     try
//     {
//         return chatInstance->loop();
//     }
//     catch (logic_error& err)
//     {
//         cerr << err.what() << endl;
//         return 0;
//     }
#endif
    return 0;
}
