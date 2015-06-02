#include <cctype>
#include <exception>

#include <boost/bind.hpp>

#include "ChatClient.h"
#include "message_formats.h"
#include "MessageBuilder.h"
#include "Logger.h"

#define CHAT_ERROR "[Chat Core]-ERROR: "

unique_ptr<ChatClient> ChatClient::_instance;
once_flag ChatClient::_onceFlag;

// Constants for FilesWatcherThread

static const uint8 SECONDS_TO_LOOSE_PACKET = 1; // seconds. Incoming packet is lost after this time (we decide).
static const uint8 ATTEMPTS_TO_RECEIVE_BLOCK = 5; // attempts. Download is interrupted after reaching this limit.
static const uint8 SECONDS_TO_WAIT_ANSWER_ON_MFI = 5; // seconds. Message with file information (M_FI) is re-sent after this time.
static const uint8 ATTEMPTS_TO_SEND_FIRST_M = 5; // attempts. We can't send M_FI after reaching this limit.
static const uint8 SECONDS_TO_BE_ALIVE = 2;

ChatClient::ChatClient() : _sendSocket(_ioService)
    , _recvSocket(_ioService)
    , _sendEndpoint(Ipv4Address::broadcast(), Port)
    , _recvEndpoint(Ipv4Address::any(), Port)
    , _runThreads(1)
    , _port(Port)
    , _fileId(0)
{
    Logger::GetInstance()->Trace("Chat client started");
    // Create handlers
    Handler::setChatInstance(this);
    _handlers.resize(LAST + 1);
    _handlers[M_SYS] = new HandlerSys;
    _handlers[M_TEXT] = new HandlerText;
    _handlers[M_FILE_BEGIN] = new HandlerFileInfo;
    _handlers[M_FILE_BLOCK] = new HandlerFileBlock;
    _handlers[M_REQ_FOR_FILE_BLOCK] = new HandlerRequestForFileBlock;
    _handlers[M_PEER_DATA] = new HandlerPeerData;

    // Set this-peer ip here
    try
    {
        boost::asio::ip::udp::resolver   resolver(_ioService);
        boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), boost::asio::ip::host_name(), "");
        boost::asio::ip::udp::resolver::iterator endpoints = resolver.resolve(query);
        UdpEndpoint ep = *endpoints;
        UdpSocket socket(_ioService);
        socket.connect(ep);

        IpAddress addr = socket.local_endpoint().address();
        string ipAddr(addr.to_string());
        Logger::GetInstance()->Trace("Using ip address ", ipAddr);
        _thisPeer.SetIp(ipAddr);
    }
    catch (exception& e)
    {
        Logger::GetInstance()->Trace(CHAT_ERROR, "Can't get local ip due to ", e.what());
    }

    Logger::GetInstance()->Trace("My peer id: ", _thisPeer.GetId());

    // Socket for sending
    _sendSocket.open(_sendEndpoint.protocol());
    _sendSocket.set_option(UdpSocket::reuse_address(true));
    _sendSocket.set_option(boost::asio::socket_base::broadcast(true));

    // Socket for receiving (including broadcast packets)
    _recvSocket.open(_recvEndpoint.protocol());
    _recvSocket.set_option(UdpSocket::reuse_address(true));
    _recvSocket.set_option(boost::asio::socket_base::broadcast(true));
    _recvSocket.bind(_recvEndpoint);
    _recvSocket.async_receive_from(
        boost::asio::buffer(_data), _recvEndpoint,
        boost::bind(&ChatClient::HandleReceiveFrom, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));

    /*
    In this thread io_service is ran
    We can't read user's input without it
    */
    ThreadsMap.insert(pair<cc_string, auto_ptr<Thread>>(BOOST_SERVICE_THREAD,
        auto_ptr<Thread>(new Thread(boost::bind(&ChatClient::BoostServiceThread, this)))));
    /*
    It is our system thread
    Here we send alive messages, send PeerData messages, track track all downloading files and send a re-send message
    if time of received block less than sender pointed out
    */
    ThreadsMap.insert(pair<cc_string, auto_ptr<Thread>>(FILESWATCHER_THREAD,
        auto_ptr<Thread>(new Thread(boost::bind(&ChatClient::ServiceFilesWatcher, this)))));

}

ChatClient::~ChatClient()
{
    DoShutdown = true;

    if (ThreadsMap[LOG_THREAD].get())
        ThreadsMap[LOG_THREAD]->join();

    if (ThreadsMap[FILESWATCHER_THREAD].get())
        ThreadsMap[FILESWATCHER_THREAD]->join();

    _recvSocket.close();
    _sendSocket.close();
    _ioService.stop();
    
    if (ThreadsMap[BOOST_SERVICE_THREAD].get())
        ThreadsMap[BOOST_SERVICE_THREAD]->join();
    
    ThreadsMap.clear();

    // Delete all downloading and sending files

    for (UploadingFilesMap::iterator it = _uploadingFiles.begin(); it != _uploadingFiles.end(); ++it)
        delete ((*it).second);

    for (SendingFilesMap::iterator it = _sendingFiles.begin(); it != _sendingFiles.end(); ++it)
        delete ((*it).second);

    // Delete all handlers

    for (Handlers::iterator it = _handlers.begin(); it != _handlers.end(); ++it)
        delete (*it);

    // Delete all peers

    for (Peers::iterator it = _peersMap.begin(); it != _peersMap.end(); ++it)
        delete((*it).second);
}

ChatClient& ChatClient::GetInstance()
{
    call_once(_onceFlag, [] {
        _instance.reset(new ChatClient);
    }
    );    
    return *_instance.get();
}

// Thread function: BOOST_SERVICE_THREAD

void ChatClient::BoostServiceThread()
{
    ErrorCode ec;

    while (_runThreads)
    {
        _ioService.run(ec);
        _ioService.reset();
    }

    if (ec)
        cout << ec.message();
}

// Thread function: FILESWATCHER_THREAD

void ChatClient::ServiceFilesWatcher()
{
    stringstream ss;
    while (_runThreads)
    {
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        {
            ScopedLock lk(_filesMutex);
            
            //scan map of peers
            for (Peers::iterator it = _peersMap.begin(); it != _peersMap.end();)
            {
                Peer* peer = (*it).second;
                if (difftime(time(0), peer->GetLastActivityCheck()) > SECONDS_TO_BE_ALIVE)
                {
                    if (!peer->WasPingSent())
                    {
                        Logger::GetInstance()->Trace("Sending ping to peer ", peer->GetId());
                        SendTo(ParseEpFromString(peer->GetIp()), MessageBuilder::System("ping", _thisPeer.GetId()));
                        peer->SetPingSent(true);
                        peer->SetPingSentTime(time(0));
                        ++it;
                        continue;
                    }
                    else if (difftime(time(0), peer->GetPingSentTime()) > SECONDS_TO_BE_ALIVE)
                    {
                        Logger::GetInstance()->Trace("Peer ", peer->GetId(), " don't answer on 'ping'. Removing it from peers map");
                        wcout << peer->GetNickname();
                        cout << " left out chat." << endl;
                        _peersMap.erase(it++);
                        delete peer;
                        continue;
                    }
                }
                ++it;
            }

            // scan map with files, that we receive
            for (UploadingFilesMap::iterator it = _uploadingFiles.begin(); it != _uploadingFiles.end();)
            {
                UploadingFilesContext * fc = (*it).second;
                if (fc == 0 || difftime(time(0), fc->ts) <= SECONDS_TO_LOOSE_PACKET)
                {
                    ++it;
                    continue;
                }

                // we have no more attempts to receive ONE block ?
                if (fc->resendCount >= ATTEMPTS_TO_RECEIVE_BLOCK)
                {
                    // delete this download
                    ss.str(string());
                    ss << "Download of " << fc->name << " ended with ERROR: peer " << fc->_recvFrom.GetId() << " is offline";
                    Logger::GetInstance()->Trace(CHAT_ERROR, ss.str());
                    cout << endl << ss.str() << endl;
                    _uploadingFiles.erase(it++);
                    fc->fp.close();

                    // delete the file
                    ErrorCode ec;
                    boost::filesystem::path abs_path = boost::filesystem::complete(fc->name);
                    boost::filesystem::remove(abs_path, ec);

                    delete fc;
                    continue;
                }

                // if we have attempts, try again
                if (fc->blocksReceived > 0)
                {
                    ss.str(string());
                    ss << "File " << fc->name << ": Request dropped packet " << fc->blocksReceived << " from " << fc->blocks;
                    Logger::GetInstance()->Trace(ss.str());
                    cout << endl << ss.str() << endl;
                }

                fc->resendCount += 1;
                SendReqForFileBlockMsg(fc);
                ++it;
            }

            // scan map with files, that we send
            // if M_FI is not sent successfully, then, if we have attempts, send it again
            for (SendingFilesMap::iterator it = _sendingFiles.begin(); it != _sendingFiles.end();)
            {
                SendingFilesContext * fsc = (*it).second;
                if (fsc == 0 || fsc->firstBlockSent || difftime(time(0), fsc->ts) <= SECONDS_TO_WAIT_ANSWER_ON_MFI)
                {
                    ++it;
                    continue;
                }

                // if we have no more attempts
                if (fsc->resendCount >= ATTEMPTS_TO_SEND_FIRST_M)
                {
                    // delete this download
                    ss.str(string());
                    ss << "Sending of ", fsc->path, " ended with ERROR: Peer doesn't request the first block";
                    Logger::GetInstance()->Trace(CHAT_ERROR, ss.str());
                    cout << endl << ss.str() << endl;
                    _sendingFiles.erase(it++);
                    delete fsc;
                    continue;
                }

                // send again

                fsc->resendCount += 1;
                SendFileInfoMsg(fsc);
                ++it;
            }
        }
    }
}

// async reading handler (udp socket)
void ChatClient::HandleReceiveFrom(const ErrorCode& err, size_t size)
{
    if (err)
        return;

    // parse received packet
    MessageSystem * pmsys = (MessageSystem*)_data.data();
    if (pmsys->_code <= LAST)
    {
        ScopedLock lk(_filesMutex);
        (_handlers[pmsys->_code])->handle(_data.data(), size);
    }

    // wait for the next message
    _recvSocket.async_receive_from(
        boost::asio::buffer(_data), _recvEndpoint,
        boost::bind(&ChatClient::HandleReceiveFrom, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}

// try to understand user's input
void ChatClient::ParseUserInput(const wstring& data)
{
    wstring tmp(data);

    // parse string
    if (tmp.size() > 1)
    {
        wstring pmCommand = tmp.substr(0, 3);
        wstring fileCommand = tmp.substr(0, 5);
        if (pmCommand == L"pm ")
        {
            // Maybe, it is the private message (PM):
            wstring nickOrIp;
            wstring msg;
            tmp.erase(0, 3);
            ParseTwoStrings(tmp, nickOrIp, msg);
            if (nickOrIp.empty() || msg.empty())
            {
                throw logic_error("invalid format.");
            }
            else
            {
                string ip = to_string(nickOrIp);
                ScopedLock lk(_filesMutex);
                if (IsIpV4(ip))
                    SendText(ParseEpFromString(ip), msg);
                else
                {
                    vector<Peer*> peers = ProcessNick(nickOrIp);
                    if (peers.size() == 0)
                    {
                        cout << "\nThere is no peer ";
                        wcout << nickOrIp << "!" << endl;
                    }                        
                    else if (peers.size() == 1)
                    {
                        SendText(ParseEpFromString(peers[0]->GetIp()), msg);
                    }
                    else
                    {
                        cout << "\nThere are several peers with nick ";
                        wcout << nickOrIp << "!" << endl;
                        for (size_t i = 0; i < peers.size(); i++)
                            cout << "\nPeer " << i << ": " << peers[i]->GetIp();
                        cout << "\nTry to send a PM directly using peer's ip." << endl;
                    }
                }
            }
            return;

        }
        else if (fileCommand == L"file ")
        {
            // Maybe, users wants to send the file
            wstring nickOrIp;
            wstring path;
            tmp.erase(0, 5);
            ParseTwoStrings(tmp, nickOrIp, path);
            if (nickOrIp.empty() || path.empty())
            {
                throw logic_error("invalid format.");
            }
            else
            {
                string ip = to_string(nickOrIp);
                ScopedLock lk(_filesMutex);
                if (IsIpV4(ip))
                    SendFile(ParseEpFromString(ip), path);
                else
                {
                    vector<Peer*> peers = ProcessNick(nickOrIp);
                    if (peers.size() == 0)
                    {
                        cout << "\nThere is no peer ";
                        wcout << nickOrIp << "!" << endl;
                    }
                    else if (peers.size() == 1)
                    {
                        SendFile(ParseEpFromString(peers[0]->GetIp()), path);
                    }
                    else
                    {
                        cout << "\nThere are several peers with nick ";
                        wcout << nickOrIp << "!" << endl;
                        for (size_t i = 0; i < peers.size(); i++)
                            cout << "\nPeer " << i << ": " << peers[i]->GetIp();
                        cout << "\nTry to send a file directly using peer's ip." << endl;
                    }
                }
            }
            return;
        }
    }

    if (tmp.empty())
        return;

    // send on broadcast address
    SendText(_sendEndpoint, tmp);
}

// get endpoint from string

UdpEndpoint ChatClient::ParseEpFromString(const string& ip)
{
    ErrorCode err;
    IpAddress addr(IpAddress::from_string(ip, err));
    if (err)
    {
        throw logic_error("can't parse this IP");
    }
    else
    {
        return UdpEndpoint(addr, _port);
    }
}

// split a string into two strings by first space-symbol

void ChatClient::ParseTwoStrings(const wstring& str, wstring& s1, wstring& s2)
{
    s1.clear();
    s2.clear();

    // try to find first space-symbol
    wstring::size_type t = str.find_first_of(L" \t");

    if (t == wstring::npos)
        return;

    s1 = wstring(str.begin(), str.begin() + t);
    s2 = str.substr(t + 1);
}

// returns a vector of pointers to Peers which have the processable nick

vector<Peer*> ChatClient::ProcessNick(const wstring& nick)
{
    vector<Peer*> peers;
    Peer* peer;
    for (Peers::const_iterator it = _peersMap.begin(); it != _peersMap.end(); it++)
    {
        peer = (*it).second;
        if (nick == peer->GetNickname())
            peers.push_back(peer);
    }

    return peers;
}

// Main loop of application

int ChatClient::loop()
{
    try
    {
        cout << "\t\t\tNice to meet you in P2P-Chat!" << endl;

        cout << "Usage:" << endl;

        cout << "1. sending broadcast message:" << endl;
        cout << "\tjust type message & press enter" << endl;

        cout << "2. sending private message:" << endl;
        cout << "\t@ [nickname] [message]" << endl;
        cout << "\t@ [ip] [message]" << endl;

        cout << "3. sending file:" << endl;
        cout << "\tfile [nickname] [path]" << endl;
        cout << "\tfile [ip] [path]" << endl;
        wstring nick;
        do 
        {
            nick.clear();
            cout << "\nPlease, type your nick > ";
            getline(wcin, nick);
        } while (nick.empty());
        _thisPeer.SetNickname(nick);

        SendPeerDataMsg(_sendEndpoint, _thisPeer.GetNickname(), _thisPeer.GetId());

        for (wstring line;;)
        {
            getline(wcin, line);
            // delete spaces to the right
            if (line.empty() == false)
            {
                wstring::iterator p;
                for (p = line.end() - 1; p != line.begin() && *p == L' '; --p);

                if (*p != L' ')
                    p++;

                line.erase(p, line.end());
            }

            if (line.empty())
                continue;
            else if (line.compare(L"quit") == 0)
            {
                Logger::GetInstance()->Trace("Shutdown");
                SendSystemMsg(_sendEndpoint, "quit");
                _runThreads = 0;
                break;
            }

            try
            {
                // parse string
                ParseUserInput(line);
            }
            catch (const logic_error& e) {
                cout << "ERR: " << e.what() << endl;
            }
        }
    }
    catch (const exception& e)
    {
        cout << e.what() << endl;
    }

    return 0;
}

// system message

void ChatClient::SendSystemMsg(const UdpEndpoint& endpoint, cc_string action)
{
    ScopedLock lk(_filesMutex);
    SendTo(endpoint, MessageBuilder::System(action, _thisPeer.GetId()));
}

void ChatClient::SendPeerDataMsg(const UdpEndpoint& endpoint, const wstring& nick, const string&id)
{
    ScopedLock lk(_filesMutex);
    SendTo(endpoint, MessageBuilder::PeerData(nick, id));
}

// text message

void ChatClient::SendText(const UdpEndpoint& endpoint, const wstring& msg)
{
    SendTo(endpoint, MessageBuilder::Text(msg, _thisPeer.GetId()));
}

// send file message

void ChatClient::SendFile(const UdpEndpoint& endpoint, const wstring& path)
{
    string filePath(path.begin(), path.end());

    ifstream input;
    input.open(filePath.c_str(), ifstream::in | ios_base::binary);
    if (!input.is_open())
    {
        cerr << "\nError! Can't open file " << filePath << endl;

        input.close();

        return;
    }
    input.close();

    // add to sent-files map
    SendingFilesContext * fsc = new SendingFilesContext;

    fsc->firstBlockSent = false;
    fsc->totalBlocks = 0;
    fsc->id = _fileId;
    fsc->path = filePath;
    fsc->endpoint = endpoint;
    fsc->endpoint.port(_port);
    fsc->resendCount = 0;
    fsc->ts = 0;
    _sendingFiles[_fileId] = fsc;
    _fileId += 1;

    Logger::GetInstance()->Trace("Start sending file ", filePath);
    cout << "\nStart sending file" << endl;
    SendFileInfoMsg(fsc);
}

// make and send block of file

void ChatClient::SendFileInfoMsg(SendingFilesContext * ctx)
{
    string fileName(ctx->path.begin(), ctx->path.end());

    ifstream input;
    input.open(fileName.c_str(), ifstream::in | ios_base::binary);
    if (!input.is_open())
        throw logic_error("Can't open file!");

    // count quantity of blocks
    uint32 blocks = 0;
    input.seekg(0, ifstream::end);
    blocks = (uint32)(input.tellg() / FILE_BLOCK_MAX);
    if (input.tellg() % FILE_BLOCK_MAX)
        blocks += 1;

    input.close();
    ctx->totalBlocks = blocks;

    // cut path (send just name)

    string::size_type t = fileName.find_last_of('\\');
    if (t != string::npos)
        fileName.erase(0, t + 1);

    // make packet and send M_FI

    SendTo(ctx->endpoint, MessageBuilder::FileBegin(ctx->id, ctx->totalBlocks, fileName, _thisPeer.GetId()));

    ctx->ts = time(0);
}

// make and send message with request of re-sending file block

void ChatClient::SendReqForFileBlockMsg(UploadingFilesContext* ctx)
{
    SendTo(ctx->endpoint, MessageBuilder::RequestForFileBlock(
        ctx->id, ctx->blocksReceived, _thisPeer.GetId()));
}

void ChatClient::SendTo(const UdpEndpoint& e, const string& m)
{
    _sendSocket.send_to(boost::asio::buffer(m), e);
}
