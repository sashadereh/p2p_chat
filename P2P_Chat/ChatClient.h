#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

#include "utils.h"
#include "Peer.h"

#include <mutex>

class ChatClient
{
public:
    virtual ~ChatClient();
    static ChatClient& GetInstance();

    int loop();

private:
    // singleton class
    static unique_ptr<ChatClient> _instance;
    static once_flag _onceFlag;
    ChatClient();
    ChatClient(const ChatClient& src);
    ChatClient& operator=(const ChatClient& rval);

    // downloading files
    struct UploadingFilesContext
    {
        UdpEndpoint endpoint;             // from
        uint blocks;              // total blocks
        uint blocksReceived;      // received blocks
        uint resendCount;         // sending requests (for one block!)
        uint id;                  // file id on the receiver side
        ofstream fp;                      // read from it
        time_t ts;                        // last block received
        string name;                      // file name
    };
    typedef map< string, UploadingFilesContext* > UploadingFilesMap;

    // sent files
    struct SentFilesContext
    {
        UdpEndpoint endpoint;               // to
        uint id;                    // file id on the sender side
        uint totalBlocks;            // total blocks
        string    path;                        // file path
        bool firstBlockSent;                   // has first block been sent?
        time_t ts;                            // first block sent
        uint resendCount;            // sending requests (for one block!)
    };
    typedef map< unsigned, SentFilesContext* > SentFilesMap;

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

    class HandlerPeerData : public Handler {
    public:
        void handle(const char* data, size_t size);
    };

    class handlerFileBegin : public Handler {
    public:
        void handle(const char* data, size_t size);
    };

    class handlerFileBlock : public Handler {
    public:
        void handle(const char* data, size_t size);
    };

    class handlerResendFileBlock : public Handler {
    public:
        void handle(const char* data, size_t size);
    };

    boost::asio::io_service _ioService;
    UdpSocket _sendSocket;
    UdpSocket _recvSocket;
    UdpEndpoint _sendEndpoint;
    UdpEndpoint _recvEndpoint;
    boost::array<char, 64 * 1024> _data;
    volatile int _runThreads;
    int _port;
    uint _fileId;
    UploadingFilesMap _files;
    SentFilesMap _filesSent;
    Mutex _filesMutex;
    Handlers _handlers;
    Peer _thisPeer;

    // async

    void ServiceThread();
    void ServiceFilesWatcher();
    void handleReceiveFrom(const ErrorCode& error, size_t bytes_recvd);

    // parsing

    UdpEndpoint parseEpFromString(const string&);
    void parseUserInput(const wstring& data);
    void parseTwoStrings(const wstring& str, string& s1, wstring& s2);
    void printSysMessage(const UdpEndpoint& endpoint, const string& msg);

    // senders

    void sendSysMsg(unsigned sysMsg);
    void SendPeerDataMsg();
    void sendMsg(const UdpEndpoint& endpoint, const wstring& message);
    void sendFile(const UdpEndpoint& endpoint, const wstring& path);
    void sendTo(const UdpEndpoint& endpoint, const string& m);
    void sendResendMsg(UploadingFilesContext* ctx);
    void sendFirstFileMsg(SentFilesContext* ctx);
};

#endif // CHAT_CLIENT_H
