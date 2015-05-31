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
    const string& GetPeerId() const { return _thisPeer.GetId(); }
    const wstring& GetPeerNick() const { return _thisPeer.GetNickname(); }

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
        Peer _recvFrom;
        UdpEndpoint endpoint;
        uint32 blocks;          // total blocks
        uint32 blocksReceived;  // received blocks
        uint32 resendCount;     // sending requests (for one block!)
        uint32 id;              // file id on the receiver side
        ofstream fp;            // read from it
        time_t ts;              // last block received
        string name;            // file name
    };
    typedef map<string, UploadingFilesContext*> UploadingFilesMap;

    // sent files
    struct SendingFilesContext
    {
        Peer _sendTo;
        UdpEndpoint endpoint;   // to
        uint32 id;              // file id on the sender side
        uint32 totalBlocks;     // total blocks
        string    path;                        // file path
        bool firstBlockSent;                   // has first block been sent?
        time_t ts;                            // first block sent
        uint32 resendCount;            // sending requests (for one block!)
        string _peerId;                 //send to it
    };
    typedef map<unsigned, SendingFilesContext*> SendingFilesMap;

    typedef map<cc_string, Peer> PeersMap;

    // Handlers

    class Handler
    {
    public:
        virtual ~Handler() {}
        virtual void handle(cc_string data, size_t size) = 0;
        static void setChatInstance(ChatClient* chatClient) { _chatClient = chatClient; }
    protected:
        static ChatClient* _chatClient;
    };
    typedef vector< Handler* > Handlers;

    class HandlerSys : public Handler {
    public:
        void handle(cc_string data, size_t size);
    };

    class HandlerText : public Handler {
    public:
        void handle(cc_string data, size_t size);
    };

    class HandlerPeerData : public Handler {
    public:
        void handle(cc_string data, size_t size);
    };

    class HandlerFileInfo : public Handler {
    public:
        void handle(cc_string data, size_t size);
    };

    class HandlerFileBlock : public Handler {
    public:
        void handle(cc_string data, size_t size);
    };

    class HandlerRequestForFileBlock : public Handler {
    public:
        void handle(cc_string data, size_t size);
    };

    boost::asio::io_service _ioService;

    UdpSocket _sendSocket;
    UdpSocket _recvSocket;
    UdpEndpoint _sendEndpoint;
    UdpEndpoint _recvEndpoint;
    uint16 _port;
    volatile uint8 _runThreads;
    boost::array<char, 64 * 1024> _data;    
    uint32 _fileId;
    UploadingFilesMap _uploadingFiles;
    SendingFilesMap _sendingFiles;
    Mutex _filesMutex;
    Mutex _peersMapMutex;
    Handlers _handlers;
    Peer _thisPeer;
    PeersMap _peersMap;

    void BoostServiceThread();
    void ServiceFilesWatcher();

    // async

    void HandleReceiveFrom(const ErrorCode& error, size_t bytes_recvd);

    // parsing

    UdpEndpoint ParseEpFromString(const string&);
    void ParseUserInput(const wstring& data);
    void ParseTwoStrings(const wstring& str, string& s1, wstring& s2);
    void PrintSystemMsg(const UdpEndpoint& endpoint, const string& msg);

    // senders

    void SendSystemMsgInternal(const UdpEndpoint& endpoint, cc_string action);
    void SendPeerDataMsg(const UdpEndpoint& endpoint, const wstring& nick, const string&id);
    void SendTextInternal(const UdpEndpoint& endpoint, const wstring& message);
    void SendFileInternal(const UdpEndpoint& endpoint, const wstring& path);
    void SendTo(const UdpEndpoint& endpoint, const string& m);
    void SendResendMsgInternal(UploadingFilesContext* ctx);
    void SendFileInfoMsgInternal(SendingFilesContext* ctx);
};

#endif // CHAT_CLIENT_H
