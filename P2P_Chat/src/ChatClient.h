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
    bool IsUsingMulticasts() { return _useMulticasts; }

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
        virtual void Handle(const char *data, size_t size) = 0;
        static void SetChatInstance(ChatClient* chatClient) { _chatClient = chatClient; }
    protected:
        static ChatClient* _chatClient;
    };
    typedef vector< Handler* > Handlers;

    class HandlerEnterMsg : public Handler {
    public:
        void Handle(const char* data, size_t size);
    };

    class HandlerQuitMsg : public Handler {
    public:
        void Handle(const char* data, size_t size);
    };

    class HandlerPingMsg : public Handler {
    public:
        void Handle(const char* data, size_t size);
    };

    class HandlerPongMsg : public Handler {
    public:
        void Handle(const char* data, size_t size);
    };

    class HandlerTextMsg : public Handler {
    public:
        void Handle(const char* data, size_t size);
    };

    enum PeerState
    {
        PS_ONLINE,
        PS_PING_SENT_ONCE,
        PS_PING_SENT_TWICE,
        PS_PING_SENT_THRICE
    };

    struct PeerInfo
    {
        time_t _lastSeen;
        string _ipAddr;
        PeerState _state;
        
        void SetOnline()
        {
            _state = PS_ONLINE;
            _lastSeen = time(NULL);
        }

        void SetNextState()
        {
            if (_state != PS_PING_SENT_THRICE)
                _state = PeerState((int)_state + 1);
        }
    };
    typedef vector<PeerInfo> Peers;
    int GetPeerIndexByIp(const string &ip);

    boost::asio::io_service _ioService;
    UdpSocket _sendSocket;
    UdpSocket _recvSocket;
    UdpEndpoint _sendBrdcastEndpoint;
    UdpEndpoint _sendMulticastEndpoint;
    UdpEndpoint _recvEndpoint;
    UdpEndpoint _localEndpoint;
    boost::array<char, 64 * 1024> _data;
    auto_ptr<Thread> _serviceThread;
    volatile int _threadsRun;
    int _port;
    bool _useMulticasts;
    Handlers _handlers;
    Timer _pingTimer;
    string _localIp;
    Peers _peers;
    Mutex _pingMutex;

    // Async
    void ServiceThread();
    void HandleReceiveFrom(const ErrorCode &error, size_t bytes_recvd);
    void CheckPingCallback(const ErrorCode &ec);

    // Parsing
    UdpEndpoint ParseEpFromString(const string&);
    void ParseUserInput(const wstring& data);
    void ParseTwoStrings(const wstring& str, string& s1, wstring& s2);
    void PrintSysMessage(const UdpEndpoint& endpoint, const string& msg);

    // Senders
    void SendSysMsg(const UdpEndpoint& endpoint, unsigned sysMsg);
    void SendMsg(const UdpEndpoint& endpoint, const wstring& message);
    void SendTo(const UdpEndpoint& endpoint, const string& m);
};

#endif // CHAT_CLIENT_H
