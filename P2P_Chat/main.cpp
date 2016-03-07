#include "ChatClient.h"
#include <boost/lexical_cast.hpp>

int main(int argc, char ** argv)
{
    bool useMulticasts = false;
    if (argc > 1)
    {
        for (int i = 1; i < argc - 1; i++)
        {
            if (strcmp(argv[i], "-p") == 0)
            {
                PORT = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-m") == 0)
            {
                useMulticasts = true;
            }
        }
    }

    ChatClient* chatInstance = &ChatClient::GetInstance();
    if (useMulticasts)
    {
        chatInstance->SetUsingMulticasts(useMulticasts);
    }

    try
    {
        return chatInstance->loop();
    }
    catch (logic_error& err)
    {
        cerr << err.what() << endl;
        return 0;
    }
}
