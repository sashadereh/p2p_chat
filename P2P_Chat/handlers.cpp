#include "ChatClient.h"
#include "MessageBuilder.h"
#include "message_formats.h"
#include "utils.h"

ChatClient* ChatClient::Handler::_chatClient = 0;

void ChatClient::handlerEnter::handle(const char*, size_t)
{
    _chatClient->printSysMessage(_chatClient->_recvEndpoint, "entered chat");
}

void ChatClient::handlerQuit::handle(const char*, size_t)
{
    _chatClient->printSysMessage(_chatClient->_recvEndpoint, "quit from chat");
}

void ChatClient::handlerText::handle(const char* data, size_t)
{
    // output to the console
    MessageText* mt = (MessageText*)data;
    cout << _chatClient->_recvEndpoint.address().to_string() << " > ";
    wcout.write(mt->text, mt->length);
    cout << endl;
}