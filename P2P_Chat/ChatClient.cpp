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

ChatClient::ChatClient() : _sendBrdcastSocket(_ioService), _sendMulticastSocket(_ioService), _recvSocket(_ioService)
    , _sendBrdcastEndpoint(Ipv4Address::broadcast(), PORT)
    , _sendMulticastEndpoint(IpAddress::from_string(MULTICAST_ADDR), PORT)
    , _recvEndpoint(Ipv4Address::any(), PORT)
    , _threadsRun(1)
    , _port(PORT)
    , _useMulticasts(false)
{
    // Get local ip
    try
    {
        boost::asio::ip::udp::resolver resolver(_ioService);
        boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), boost::asio::ip::host_name(), "");
        boost::asio::ip::udp::resolver::iterator endpoints = resolver.resolve(query);
        UdpEndpoint ep = *endpoints;
        UdpSocket socket(_ioService);
        socket.connect(ep);

        IpAddress addr = socket.local_endpoint().address();
        std::cout << "Chat client started. Using ip = " << addr.to_string() << ", listening port = " << PORT << std::endl;
    }
    catch (exception& e)
    {
        std::cerr << "Could not get local ip. Exception: " << e.what() << endl;
        exit(-1);
    }

    // Create handlers
    Handler::setChatInstance(this);
    _handlers.resize(MT_LAST + 1);
    _handlers[MT_ENTER] = new handlerEnter;
    _handlers[MT_QUIT] = new handlerQuit;
    _handlers[MT_TEXT] = new handlerText;

    // Socket for sending broadcasts
    _sendBrdcastSocket.open(_sendBrdcastEndpoint.protocol());
    _sendBrdcastSocket.set_option(UdpSocket::reuse_address(true));
    _sendBrdcastSocket.set_option(boost::asio::socket_base::broadcast(true));

    // Socket for sending multicasts

    // Socket for receiving (including broadcast/multicast packets)
    _recvSocket.open(_recvEndpoint.protocol());
    _recvSocket.set_option(UdpSocket::reuse_address(true));
    _recvSocket.set_option(boost::asio::socket_base::broadcast(true));
    _recvSocket.bind(_recvEndpoint);
    _recvSocket.async_receive_from(
        boost::asio::buffer(_data), _recvEndpoint,
        boost::bind(&ChatClient::handleReceiveFrom, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));

    /*
    In this thread io_service is ran
    We can't read user's input without it
    */
    _serviceThread.reset(new Thread(boost::bind(&ChatClient::serviceThread, this)));
}

ChatClient::~ChatClient()
{
    _threadsRun = 0;

    _recvSocket.close();
    _sendBrdcastSocket.close();
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

// Thread function

void ChatClient::serviceThread()
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

// async reading handler (udp socket)
void ChatClient::handleReceiveFrom(const ErrorCode& err, size_t size)
{
    if (err) return;

    // parse received packet
    MessageSys * receivedMsg = (MessageSys*)_data.data();
    if (receivedMsg->code <= MT_LAST)
    {
        (_handlers[receivedMsg->code])->handle(_data.data(), size);
    }

    // wait for the next message
    _recvSocket.async_receive_from(
        boost::asio::buffer(_data), _recvEndpoint,
        boost::bind(&ChatClient::handleReceiveFrom, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}

// try to understand user's input
void ChatClient::parseUserInput(const wstring& data)
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
            parseTwoStrings(tmp, ip, msg);
            if (ip.empty() || msg.empty())
            {
                throw logic_error("invalid format.");
            }
            else
            {
                sendMsg(parseEpFromString(ip), msg);
            }
            return;

        }
        else if (tmp.find(L"mc ") == 0)
        {
            /*
            // Maybe, users wants to send the file:
            // file ip path
            tmp.erase(0, 5);
            string  ip;
            wstring path;
            parseTwoStrings(tmp, ip, path);
            if (ip.empty() || path.empty())
            {
                throw logic_error("invalid format.");
            }
            else {
                sendFile(parseEpFromString(ip), path);
            }
            return;
            */
        }
    }

    if (tmp.empty()) return;

    // send on broadcast address
    sendMsg(_sendBrdcastEndpoint, tmp);
}

// get endpoint from string

UdpEndpoint ChatClient::parseEpFromString(const string& ip)
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

void ChatClient::printSysMessage(const UdpEndpoint& endpoint, const string& msg)
{
    cout << endpoint.address().to_string() << " > " << "*** " << msg << " ***" << endl;
}

// Split a string into two strings by first space-symbol
void ChatClient::parseTwoStrings(const wstring& tmp, string& s1, wstring& s2)
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
        sendSysMsg(MT_ENTER);
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
                parseUserInput(line);
            }
            catch (const logic_error& e)
            {
                cout << "ERROR: " << e.what() << endl;
            }
        }
        sendSysMsg(MT_QUIT);
    }
    catch (const exception& e)
    {
        cout << e.what() << endl;
    }

    return 0;
}

// System message
void ChatClient::sendSysMsg(unsigned sysMsg)
{
    sendTo(_sendBrdcastEndpoint, MessageBuilder::system(sysMsg));
}

// Text message
void ChatClient::sendMsg(const UdpEndpoint& endpoint, const wstring& msg)
{
    sendTo(endpoint, MessageBuilder::text(msg));
}

void ChatClient::sendTo(const UdpEndpoint& e, const string& m)
{
    _sendBrdcastSocket.send_to(boost::asio::buffer(m), e);
}

void ChatClient::SetUsingMulticasts(bool usingMulticasts)
{
    _useMulticasts = usingMulticasts;
    std::cout << "Multicasts enabled. Print \"mc message\" to send a multicast message." << std::endl;
    // Join multicasts group
}