#include "ChatClient.h"
#include "Logger.h"
#include <boost/lexical_cast.hpp>

int main(int argc, char ** argv)
{
    if (argc > 1)
    {
        try
        {
            Port = boost::lexical_cast<int>(argv[1]);
        }
        catch (const boost::bad_lexical_cast& err)
        {
            cerr << err.what();
        }
    }

    ChatClient* chatInstance = &ChatClient::GetInstance();

    try
    {
        return chatInstance->loop();
    }
    catch (logic_error& err)
    {
        cerr << err.what() << endl;
        return 1;
    }
}
