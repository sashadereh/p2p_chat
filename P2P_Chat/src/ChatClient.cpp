#include <iostream>
#include <string>
#include <sstream>
#include <cctype>
#include <exception>
#include <fstream>
#include <ctime>

#include <boost/bind.hpp>

#include "ChatClient.h"
#include "message_formats.h"
#include "MessageBuilder.h"

unique_ptr<ChatClient> ChatClient::_instance;
once_flag ChatClient::_onceFlag;

ChatClient::ChatClient() : _sendSocket(_ioService), _recvSocket(_ioService)
    , _sendBrdcastEndpoint(Ipv4Address::broadcast(), PORT)
    , _sendMulticastEndpoint(IpAddress::from_string(MULTICAST_ADDR), PORT)
    , _recvEndpoint(Ipv4Address::any(), PORT)
    , _threadsRun(1)
    , _port(PORT)
    , _useMulticasts(false)
    , _pingTimer(_ioService, boost::posix_time::seconds(PING_TIMER_FREQ))
{
#if defined _WIN32
    // Get local ip
    try
    {
        boost::asio::ip::udp::resolver resolver(_ioService);
        boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), boost::asio::ip::host_name(), "");
        boost::asio::ip::udp::resolver::iterator endpoints = resolver.resolve(query);
        UdpEndpoint ep = *endpoints;
        UdpSocket socket(_ioService);
        socket.connect(ep);

        _localEndpoint = socket.local_endpoint();
        IpAddress addr = _localEndpoint.address();
        _localIp = addr.to_string();
        cout << "Chat client started. Using ip = " << _localIp << ", listening port = " << PORT << endl;
    }
    catch (exception& e)
    {
        cerr << "Could not get local ip. Exception: " << e.what() << endl;
        exit(-1);
    }
#elif defined __linux
    cout << "Chat client started. Listening port = " << PORT << endl;
#endif

    // Create handlers
    Handler::SetChatInstance(this);
    _handlers.resize(MT_LAST + 1);
    _handlers[MT_ENTER] = new HandlerEnterMsg;
    _handlers[MT_QUIT] = new HandlerQuitMsg;
    _handlers[MT_PING] = new HandlerPingMsg;
    _handlers[MT_PONG] = new HandlerPongMsg;
    _handlers[MT_TEXT] = new HandlerTextMsg;

    // Socket for sending broadcasts
    _sendSocket.open(_sendBrdcastEndpoint.protocol());
    _sendSocket.set_option(UdpSocket::reuse_address(true));
    _sendSocket.set_option(boost::asio::socket_base::broadcast(true));
#if defined _WIN32
    _sendSocket.bind(_localEndpoint);
#endif

    // Socket for receiving (including broadcast/multicast packets)
    _recvSocket.open(_recvEndpoint.protocol());
    _recvSocket.set_option(UdpSocket::reuse_address(true));
    _recvSocket.set_option(boost::asio::socket_base::broadcast(true));
    _recvSocket.bind(_recvEndpoint);
    _recvSocket.async_receive_from(
        boost::asio::buffer(_data), _recvEndpoint,
        boost::bind(&ChatClient::HandleReceiveFrom, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));

    // Timer for checking if peers still alive (ping)
    _pingTimer.async_wait(boost::bind(&ChatClient::CheckPingCallback, this, boost::asio::placeholders::error));

    /*
    In this thread io_service is ran
    We can't read user's input without it
    */
    _serviceThread.reset(new Thread(boost::bind(&ChatClient::ServiceThread, this)));
}

ChatClient::~ChatClient()
{
    _threadsRun = 0;

    _recvSocket.close();
    _sendSocket.close();
    _ioService.stop();

    // Wait for the completion of the service thread

    if (_serviceThread.get())
        _serviceThread->join();

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

int ChatClient::GetPeerIndexByIp(const string &ip)
{
    for (Peers::const_iterator it = _peers.begin(); it != _peers.end(); it++)
    {
        if (it->_ipAddr == ip)
            return it - _peers.begin();
    }

    return -1;
}

// Thread function
void ChatClient::ServiceThread()
{
    ErrorCode errCode;

    while (_threadsRun)
    {
        _ioService.run(errCode);
        _ioService.reset();
    }

    if (errCode)
        cerr << errCode.message();
}

// Async reading handler (udp socket)
void ChatClient::HandleReceiveFrom(const ErrorCode& err, size_t size)
{
    if (err)
        return;

    // parse received packet
    MessageSys * receivedMsg = (MessageSys*)_data.data();
    if (receivedMsg->code <= MT_LAST)
    {
        ScopedLock lk(_pingMutex);
        string remoteIp = _recvEndpoint.address().to_string();
        int peerIndex = GetPeerIndexByIp(remoteIp);
        if (remoteIp != _localIp && peerIndex == -1)
        {
            PeerInfo newPeerInfo;
            newPeerInfo._ipAddr = remoteIp;
            newPeerInfo.SetOnline();
            _peers.push_back(newPeerInfo);
        }
        else if (peerIndex != -1)
        {            
            _peers.at(peerIndex).SetOnline();
        }

        (_handlers[receivedMsg->code])->Handle(_data.data(), size);
    }
    else
    {
        cerr << "ERROR: Received message with invalid code!" << endl;
    }

    // wait for the next message
    _recvSocket.async_receive_from(
        boost::asio::buffer(_data), _recvEndpoint,
        boost::bind(&ChatClient::HandleReceiveFrom, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}

// Try to understand user's input
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
            else
            {
                SendMsg(ParseEpFromString(ip), msg);
            }
            return;

        }
        else if (tmp.find(L"mc ") == 0)
        {
            // Maybe, users wants to send multicast msg:
            // mc message
            if (_instance->IsUsingMulticasts())
            {                
                tmp.erase(0, 3);
                SendMsg(_sendMulticastEndpoint, tmp);
            }
            else
            {
                cout << "Multicasts are not enabled. Enable it by passing \"-m\" as cmd arg" << endl;
            }
            return;
        }
    }

    if (tmp.empty())
        return;

    // send on broadcast address
    SendMsg(_sendBrdcastEndpoint, tmp);
}

// Get endpoint from string
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

void ChatClient::PrintSysMessage(const UdpEndpoint& endpoint, const string& msg)
{
    cout << endpoint.address().to_string() << " > " << "*** " << msg << " ***" << endl;
}

// Split a string into two strings by first space-symbol
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

// Read user's input
int ChatClient::Loop()
{
    try
    {
        SendSysMsg(_sendBrdcastEndpoint, MT_ENTER);
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
            catch (const logic_error& e)
            {
                cout << "ERROR: " << e.what() << endl;
            }
        }
        SendSysMsg(_sendBrdcastEndpoint, MT_QUIT);
    }
    catch (const exception& e)
    {
        cout << e.what() << endl;
    }

    return 0;
}

// System message
void ChatClient::SendSysMsg(const UdpEndpoint& endpoint, unsigned sysMsg)
{
    SendTo(endpoint, MessageBuilder::system(sysMsg));
}

// Text message
void ChatClient::SendMsg(const UdpEndpoint& endpoint, const wstring& msg)
{
    SendTo(endpoint, MessageBuilder::text(msg));
}

void ChatClient::SendTo(const UdpEndpoint& e, const string& m)
{
    _sendSocket.send_to(boost::asio::buffer(m), e);
}

void ChatClient::SetUsingMulticasts(bool usingMulticasts)
{
    _useMulticasts = usingMulticasts;
    if (_useMulticasts)
    {
        cout << "Multicasts enabled. Print \"mc message\" to send a multicast message" << endl;
        _recvSocket.set_option(boost::asio::ip::multicast::join_group(IpAddress::from_string(MULTICAST_ADDR)));
    }
    else
    {
        cout << "Multicasts disabled" << endl;
        _recvSocket.set_option(boost::asio::ip::multicast::leave_group(IpAddress::from_string(MULTICAST_ADDR)));
    }
}

void ChatClient::CheckPingCallback(const ErrorCode &ec)
{
    ScopedLock lk(_pingMutex);
    Peers::iterator it = _peers.begin();
    while (it != _peers.end())
    {
        time_t lastSeen = it->_lastSeen;
        if (time(NULL) - lastSeen > START_PING_INTERVAL)
        {
            PeerState peerState = it->_state;
            UdpEndpoint peerEndpoint = UdpEndpoint(IpAddress::from_string(it->_ipAddr), PORT);
            if (peerState != PS_PING_SENT_THRICE)
            {
                SendSysMsg(peerEndpoint, MT_PING);
                it->SetNextState();
                it++;
            }
            else
            {
                PrintSysMessage(peerEndpoint, "quit from chat");
                it = _peers.erase(it);
            }
        }
        else
            it++;
    }

    _pingTimer.expires_at(_pingTimer.expires_at() + boost::posix_time::seconds(PING_TIMER_FREQ));
    _pingTimer.async_wait(boost::bind(&ChatClient::CheckPingCallback, this, boost::asio::placeholders::error));

}
