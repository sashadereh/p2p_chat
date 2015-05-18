#include <cctype>
#include <exception>

#include <boost/bind.hpp>

#include "ChatClient.h"
#include "message_formats.h"
#include "MessageBuilder.h"
#include "Logger.h"

unique_ptr<ChatClient> ChatClient::_instance;
once_flag ChatClient::_onceFlag;

// Constants for FilesWatcherThread

static const short int SECONDS_TO_LOOSE_PACKET = 1; // seconds. Incoming packet is lost after this time (we decide).
static const short int ATTEMPTS_TO_RECEIVE_BLOCK = 5; // attempts. Download is interrupted after reaching this limit.
static const short int SECONDS_TO_WAIT_ANSWER = 5; // seconds. First message with file information (FMWFI) is re-sent after this time.
static const short int ATTEMPTS_TO_SEND_FIRST_M = 5; // attempts. We can't send FMWFI after reaching this limit.

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
    _handlers[M_FILE_BEGIN] = new HandlerFileBegin;
    _handlers[M_FILE_BLOCK] = new HandlerFileBlock;
    _handlers[M_RESEND_FILE_BLOCK] = new HandlerResendFileBlock;
    _handlers[M_PEER_DATA] = new HandlerPeerData;

    // Set local ip here
    boost::asio::ip::udp::resolver resolver(_ioService);
    boost::asio::ip::udp::resolver::query query(boost::asio::ip::host_name(), "");
    boost::asio::ip::udp::resolver::iterator it = resolver.resolve(query);
    boost::asio::ip::udp::endpoint endpoint = *it;
    _thisPeer.SetIp(endpoint.address().to_string());

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
    ThreadsMap.insert(pair<cc_string, auto_ptr<Thread>>(SERVICE_THREAD,
        auto_ptr<Thread>(new Thread(boost::bind(&ChatClient::ServiceThread, this)))));
    /*
    This thread track all downloading files and send a re-send message
    if time of received block less than sender pointed out
    */
    ThreadsMap.insert(pair<cc_string, auto_ptr<Thread>>(FILESWATCHER_THREAD,
        auto_ptr<Thread>(new Thread(boost::bind(&ChatClient::ServiceFilesWatcher, this)))));
}

ChatClient::~ChatClient()
{
    _runThreads = 0;
    DoShutdown = true;

    _recvSocket.close();
    _sendSocket.close();
    _ioService.stop();

    // Wait for the completion of the threads

    if (ThreadsMap[LOG_THREAD].get())
        ThreadsMap[LOG_THREAD]->join();

    if (ThreadsMap[FILESWATCHER_THREAD].get())
        ThreadsMap[FILESWATCHER_THREAD]->join();

    if (ThreadsMap[SERVICE_THREAD].get())
        ThreadsMap[SERVICE_THREAD]->join();
    
    ThreadsMap.clear();

    // Delete all downloading and sending files

    for (UploadingFilesMap::iterator it = _files.begin(); it != _files.end(); ++it)
        delete ((*it).second);

    for (SentFilesMap::iterator it = _filesSent.begin(); it != _filesSent.end(); ++it)
        delete ((*it).second);

    // Delete all handlers

    for (Handlers::iterator it = _handlers.begin(); it != _handlers.end(); ++it)
        delete (*it);
}

ChatClient& ChatClient::GetInstance()
{
    call_once(_onceFlag, [] {
        _instance.reset(new ChatClient);
    }
    );    
    return *_instance.get();
}

// Thread function

void ChatClient::ServiceThread()
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

// Thread function

void ChatClient::ServiceFilesWatcher()
{
    stringstream ss;
    while (_runThreads)
    {
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        {
            ScopedLock lk(_filesMutex);
            // files we receive
            for (UploadingFilesMap::iterator it = _files.begin(); it != _files.end();)
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
                    PrintSystemMsg(fc->endpoint, "downloading ended with error!");
                    _files.erase(it++);
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

                    ss << "request dropped packet " << fc->blocksReceived << " from " << fc->blocks;
                    PrintSystemMsg(fc->endpoint, ss.str());
                }

                fc->resendCount += 1;
                SendResendMsgInternal(fc);
                ++it;
            }
            // files we send
            // if FMWFI is not sent successfully, then, if we have attempts, send it again
            for (SentFilesMap::iterator it = _filesSent.begin(); it != _filesSent.end();)
            {
                SentFilesContext * fsc = (*it).second;
                if (fsc == 0 || fsc->firstBlockSent || difftime(time(0), fsc->ts) <= SECONDS_TO_WAIT_ANSWER)
                {
                    ++it;
                    continue;
                }

                // if we have no more attempts
                if (fsc->resendCount >= ATTEMPTS_TO_SEND_FIRST_M)
                {
                    // delete this download
                    PrintSystemMsg(fsc->endpoint, "sending ended with ERROR");
                    _filesSent.erase(it++);
                    delete fsc;
                    continue;
                }

                // send again

                fsc->resendCount += 1;
                SendFileInfoMsgInternal(fsc);
                ++it;
            }
        }
    }
}

// async reading handler (udp socket)
void ChatClient::HandleReceiveFrom(const ErrorCode& err, size_t size)
{
    if (err) return;

    // parse received packet
    MessageSys * pmsys = (MessageSys*)_data.data();
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
        if (tmp.at(0) == L'*')
        {
            // no asterisks (system message have them)
            for (unsigned t = 0; t < tmp.size() && tmp[t] == L'*'; ++t);
        }
        else if (tmp.at(0) == L'@')
        {
            // Maybe, it is the private message (PM) such format:
            // @ip message
            string  ip;
            wstring msg;
            tmp.erase(0, 1);
            ParseTwoStrings(tmp, ip, msg);
            if (ip.empty() || msg.empty())
            {
                throw logic_error("invalid format.");
            }
            else {
                SendTextInternal(ParseEpFromString(ip), msg);
            }
            return;

        }
        else if (tmp.find(L"file ") == 0)
        {
            // Maybe, users wants to send the file:
            // file ip path
            tmp.erase(0, 5);
            string  ip;
            wstring path;
            ParseTwoStrings(tmp, ip, path);
            if (ip.empty() || path.empty())
            {
                throw logic_error("invalid format.");
            }
            else {
                SendFileInternal(ParseEpFromString(ip), path);
            }
            return;
        }
    }

    if (tmp.empty()) return;

    // send on broadcast address
    SendTextInternal(_sendEndpoint, tmp);
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
    else {
        return UdpEndpoint(addr, _port);
    }
}

// print system message

void ChatClient::PrintSystemMsg(const UdpEndpoint& endpoint, const string& msg)
{
    cout << endpoint.address().to_string() << " > " << "*** " << msg << " ***" << endl;
}

// split a string into two strings by first space-symbol

void ChatClient::ParseTwoStrings(const wstring& tmp, string& s1, wstring& s2)
{
    s1.clear();
    s2.clear();

    // try to find first space-symbol
    wstring::size_type t = tmp.find_first_of(L" \t");

    if (t == wstring::npos)
        return;

    s1 = string(tmp.begin(), tmp.begin() + t);
    s2 = tmp.substr(t + 1);
}

// Main loop of application

int ChatClient::loop()
{
    try
    {
        cout << "Hello! Please, type your nick > ";
        wstring nick;
        getline(wcin, nick);
        _thisPeer.SetNickname(nick);
        SendSystemMsgInternal("enter");
        SendPeerDataMsg();
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
                break;

            try
            {
                // parse string
                ParseUserInput(line);
            }
            catch (const logic_error& e) {
                cout << "ERR: " << e.what() << endl;
            }
        }
        SendSystemMsgInternal("quit");
    }
    catch (const exception& e) {
        cout << e.what() << endl;
    }

    return 0;
}

// system message

void ChatClient::SendSystemMsgInternal(cc_string action)
{
    ScopedLock lk(_filesMutex);
    SendTo(_sendEndpoint, MessageBuilder::System(action, PEER_ID));
}

void ChatClient::SendPeerDataMsg()
{
    ScopedLock lk(_filesMutex);
    SendTo(_sendEndpoint, MessageBuilder::PeerData(_thisPeer.GetNickname(), _thisPeer.GetId()));
}

// text message

void ChatClient::SendTextInternal(const UdpEndpoint& endpoint, const wstring& msg)
{
    ScopedLock lk(_filesMutex);
    SendTo(endpoint, MessageBuilder::Text(msg, PEER_ID));
}

// send file message

void ChatClient::SendFileInternal(const UdpEndpoint& endpoint, const wstring& path)
{
    ScopedLock lk(_filesMutex);
    string fileName(path.begin(), path.end());

    ifstream input;
    input.open(fileName.c_str(), ifstream::in | ios_base::binary);
    if (!input.is_open())
    {
        cerr << "Error! Can't open file!" << endl;

        input.close();

        return;
    }
    input.close();

    // add to sent-files map
    SentFilesContext * fsc = new SentFilesContext;

    fsc->firstBlockSent = false;
    fsc->totalBlocks = 0;
    fsc->id = _fileId;
    fsc->path = fileName;
    fsc->endpoint = endpoint;
    fsc->endpoint.port(_port);
    fsc->resendCount = 0;
    fsc->ts = 0;
    _filesSent[_fileId] = fsc;
    _fileId += 1;

    SendFileInfoMsgInternal(fsc);
}

// make and send block of file

void ChatClient::SendFileInfoMsgInternal(SentFilesContext * ctx)
{
    string fileName(ctx->path.begin(), ctx->path.end());

    ifstream input;
    input.open(fileName.c_str(), ifstream::in | ios_base::binary);
    if (!input.is_open())
        throw logic_error("Can't open file!");

    // count quantity of blocks
    unsigned blocks = 0;
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

    // make packet and send FMWFI

    SendTo(ctx->endpoint, MessageBuilder::FileBegin(ctx->id, ctx->totalBlocks, fileName));

    ctx->ts = time(0);
}

// make and send message with request of re-sending file block

void ChatClient::SendResendMsgInternal(UploadingFilesContext* ctx)
{
    SendTo(ctx->endpoint, MessageBuilder::ResendableFileBlock(
        ctx->id, ctx->blocksReceived));
}

void ChatClient::SendTo(const UdpEndpoint& e, const string& m)
{
    _sendSocket.send_to(boost::asio::buffer(m), e);
}
