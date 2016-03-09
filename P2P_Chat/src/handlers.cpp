#include "ChatClient.h"
#include "MessageBuilder.h"
#include "message_formats.h"
#include "utils.h"

ChatClient* ChatClient::Handler::_chatClient = 0;

void ChatClient::HandlerEnterMsg::Handle(const char*, size_t)
{
    _chatClient->PrintSysMessage(_chatClient->_recvEndpoint, "entered chat");
}

void ChatClient::HandlerQuitMsg::Handle(const char*, size_t)
{
    _chatClient->PrintSysMessage(_chatClient->_recvEndpoint, "quit from chat");
}

void ChatClient::HandlerPingMsg::Handle(const char*, size_t)
{
    UdpEndpoint remoteEndpoint(_chatClient->_recvEndpoint.address(), PORT);
    _chatClient->SendSysMsg(remoteEndpoint, MT_PONG);
}

void ChatClient::HandlerPongMsg::Handle(const char*, size_t)
{
    // It is just a stub. We set PS_ONLINE and last seen time for this peer before calling this handler
}

void ChatClient::HandlerTextMsg::Handle(const char* data, size_t)
{
    // output to the console
    MessageText* mt = (MessageText*)data;
    cout << _chatClient->_recvEndpoint.address().to_string() << " > ";
    wcout.write(mt->text, mt->length);
    cout << endl;
}