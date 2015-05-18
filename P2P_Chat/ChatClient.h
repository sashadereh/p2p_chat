#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

#include "utils.h"
#include "Peer.h"
#include "Handlers.h"

#include <mutex>

class ChatClient
{
public:
    virtual ~ChatClient();
    static ChatClient& GetInstance();
    const string& GetPeerId() const { return _thisPeer.GetId(); }
    const wstring& GetPeerNick() const { return _thisPeer.GetNickname(); }
    void SendSystemMessage();

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
        uint32 blocks;              // total blocks
        uint32 blocksReceived;      // received blocks
        uint32 resendCount;         // sending requests (for one block!)
        uint32 id;                  // file id on the receiver side
        ofstream fp;                      // read from it
        time_t ts;                        // last block received
        string name;                      // file name
    };
    typedef map< string, UploadingFilesContext* > UploadingFilesMap;

    // sent files
    struct SentFilesContext
    {
        UdpEndpoint endpoint;               // to
        uint32 id;                    // file id on the sender side
        uint32 totalBlocks;            // total blocks
        string    path;                        // file path
        bool firstBlockSent;                   // has first block been sent?
        time_t ts;                            // first block sent
        uint32 resendCount;            // sending requests (for one block!)
    };
    typedef map< unsigned, SentFilesContext* > SentFilesMap;

    // Handlers

    boost::asio::io_service _ioService;

    UdpSocket _sendSocket;
    UdpSocket _recvSocket;
    UdpEndpoint _sendEndpoint;
    UdpEndpoint _recvEndpoint;
    uint32 _port;
    volatile uint8 _runThreads;
    boost::array<char, 64 * 1024> _data;    
    uint32 _fileId;
    UploadingFilesMap _files;
    SentFilesMap _filesSent;
    Mutex _filesMutex;
    Handlers _handlers;
    Peer _thisPeer;
    PeersMap _peersMap;

    // async

    void ServiceThread();
    void ServiceFilesWatcher();
    void HandleReceiveFrom(const ErrorCode& error, size_t bytes_recvd);

    // parsing

    UdpEndpoint ParseEpFromString(const string&);
    void ParseUserInput(const wstring& data);
    void ParseTwoStrings(const wstring& str, string& s1, wstring& s2);
    void PrintSystemMsg(const UdpEndpoint& endpoint, const string& msg);

    // senders

    void SendSystemMsgInternal(cc_string action);
    void SendPeerDataMsg();
    void SendTextInternal(const UdpEndpoint& endpoint, const wstring& message);
    void SendFileInternal(const UdpEndpoint& endpoint, const wstring& path);
    void SendTo(const UdpEndpoint& endpoint, const string& m);
    void SendResendMsgInternal(UploadingFilesContext* ctx);
    void SendFileInfoMsgInternal(SentFilesContext* ctx);
};

#endif // CHAT_CLIENT_H
