#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

#include "utils.h"

#include <mutex>

class ChatClient
{
public:
    virtual ~ChatClient();
    static ChatClient& GetInstance();

    int Loop();
    void SetUsingMulticasts(bool usingMulticasts);

private:
    // singleton class
    static unique_ptr<ChatClient> _instance;
    static once_flag _onceFlag;
    ChatClient();
    ChatClient(const ChatClient& src);
    ChatClient& operator=(const ChatClient& rval);

    // Handlers
    class Handler
    {
    public:
        virtual ~Handler() {}
        virtual void handle(const char *data, size_t size) = 0;
        static void setChatInstance(ChatClient* chatClient) { _chatClient = chatClient; }
    protected:
        static ChatClient* _chatClient;
    };
    typedef vector< Handler* > Handlers;

    class handlerEnter : public Handler {
    public:
        void handle(const char* data, size_t size);
    };

    class handlerQuit : public Handler {
    public:
        void handle(const char* data, size_t size);
    };

    class handlerText : public Handler {
    public:
        void handle(const char* data, size_t size);
    };

    boost::asio::io_service _ioService;
    UdpSocket _sendBrdcastSocket;
    UdpSocket _sendMulticastSocket;
    UdpSocket _recvSocket;
    UdpEndpoint _sendBrdcastEndpoint;
    UdpEndpoint _sendMulticastEndpoint;
    UdpEndpoint _recvEndpoint;
    boost::array<char, 64 * 1024> _data;
    auto_ptr<Thread> _serviceThread;
    volatile int _threadsRun;
    int _port;
    bool _useMulticasts;
    Handlers _handlers;

    // async

    void serviceThread();
    void handleReceiveFrom(const ErrorCode& error, size_t bytes_recvd);

    // parsing

    UdpEndpoint parseEpFromString(const string&);
    void parseUserInput(const wstring& data);
    void parseTwoStrings(const wstring& str, string& s1, wstring& s2);
    void printSysMessage(const UdpEndpoint& endpoint, const string& msg);

    // senders

    void sendSysMsg(unsigned sysMsg);
    void sendMsg(const UdpEndpoint& endpoint, const wstring& message);
    void sendTo(const UdpEndpoint& endpoint, const string& m);
};

#endif // CHAT_CLIENT_H
