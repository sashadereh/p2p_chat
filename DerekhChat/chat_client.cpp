#include <iostream>
#include <string>
#include <sstream>
#include <cctype>
#include <exception>
#include <fstream>
#include <ctime>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/array.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/filesystem.hpp>

#include "chat_client.h"
#include "message_formats.h"
#include "message_builder.h"

// Constants for FilesWatcherThread

static const short int SECONDS_TO_LOOSE_PACKET = 1; // seconds. Incoming packet is lost after this time (we decide).
static const short int ATTEMPTS_TO_RECEIVE_BLOCK = 5; // attempts. Download is interrupted after reaching this limit.
static const short int SECONDS_TO_WAIT_ANSWER = 5; // seconds. First message with file information (FMWFI) is re-sent after this time.
static const short int ATTEMPTS_TO_SEND_FIRST_M = 5; // attempts. We can't send FMWFI after reaching this limit.

ChatClient::ChatClient(int port)
	: _sendSocket(_ioService)
	, _recvSocket(_ioService)
	, _sendEndpoint(boost::asio::ip::address_v4::broadcast(), port)
	, _recvEndpoint(boost::asio::ip::address_v4::any(), port)
	, _threadsRunned(1)
	, _port(port)
	, _fileId(0)
{
	// Create handlers
	Handler::setChatInstance(this);
	_handlers.resize(msgLast + 1);
	_handlers[msgEnter] = new handlerEnter;
	_handlers[msgQuit] = new handlerQuit;
	_handlers[msgText] = new handlerText;
	_handlers[msgFileBegin] = new handlerFileBegin;
	_handlers[msgFileBlock] = new handlerFileBlock;
	_handlers[msgResendFileBlock] = new handlerResendFileBlock;

	// Socket for sending
	_sendSocket.open(_sendEndpoint.protocol());
	_sendSocket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
	_sendSocket.set_option(boost::asio::socket_base::broadcast(true));

	// Socket for receiving (including broadcast packets)
	_recvSocket.open(_recvEndpoint.protocol());
	_recvSocket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
	_recvSocket.set_option(boost::asio::socket_base::broadcast(true));
	_recvSocket.bind(_recvEndpoint);
	_recvSocket.async_receive_from(
		boost::asio::buffer(_data), _recvEndpoint,
		boost::bind(&ChatClient::handleReceiveFrom, this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));

	/*
	In this thread io_service is ran
	We can't read users's input without it
	*/
	_serviceThread.reset(new boost::thread(boost::bind(&ChatClient::serviceThread, this)));

	/*
	This thread track all downloading files and send a re-send message
	if time of received block less than sender pointed out
	*/
	_watcherThread.reset(new boost::thread(boost::bind(&ChatClient::serviceFilesWatcher, this)));
}

ChatClient::~ChatClient()
{
	_threadsRunned = 0;

	_recvSocket.close();
	_sendSocket.close();
	_ioService.stop();

	// Wait for the completion of the threads

	if (_watcherThread.get())
		_watcherThread->join();

	if (_serviceThread.get())
		_serviceThread->join();

	// Delete all downloading and sending files

	for (FilesMap::iterator it = _files.begin(); it != _files.end(); ++it)
		delete ((*it).second);

	for (FilesSentMap::iterator it = _filesSent.begin(); it != _filesSent.end(); ++it)
		delete ((*it).second);

	// Delete all handlers

	for (Handlers::iterator it = _handlers.begin(); it != _handlers.end(); ++it)
		delete (*it);
}

// Thread function

void ChatClient::serviceThread()
{
	boost::system::error_code ec;

	while (_threadsRunned)
	{
		_ioService.run(ec);
		_ioService.reset();
	}

	if (ec)
		std::cout << ec.message();
}

// Thread function

void ChatClient::serviceFilesWatcher()
{
	std::stringstream ss;
	while (_threadsRunned)
	{
		boost::this_thread::sleep(boost::posix_time::seconds(1));
		{
			boost::mutex::scoped_lock lk(_filesMutex);
			// files we receive
			for (FilesMap::iterator it = _files.begin(); it != _files.end();)
			{
				FileContext * fc = (*it).second;
				if (fc == 0 || std::difftime(std::time(0), fc->ts) <= SECONDS_TO_LOOSE_PACKET)
				{
					++it;
					continue;
				}

				// we have no more attempts to receive ONE block ?
				if (fc->resendCount >= ATTEMPTS_TO_RECEIVE_BLOCK)
				{
					// delete this download
					printSysMessage(fc->endpoint, "downloading ended with error!");
					_files.erase(it++);
					fc->fp.close();

					// delete the file
					boost::system::error_code ec;
					boost::filesystem::path abs_path = boost::filesystem::complete(fc->name);
					boost::filesystem::remove(abs_path, ec);

					delete fc;
					continue;
				}

				// if we have attempts, try again
				if (fc->blocksReceived > 0)
				{
					ss.str(std::string());

					ss << "request dropped packet " << fc->blocksReceived << " from " << fc->blocks;
					printSysMessage(fc->endpoint, ss.str());
				}

				fc->resendCount += 1;
				sendResendMsg(fc);
				++it;
			}
			// files we send
			// if FMWFI is not sent successfully, then, if we have attempts, send it again
			for (FilesSentMap::iterator it = _filesSent.begin(); it != _filesSent.end();)
			{
				FilesSentContext * fsc = (*it).second;
				if (fsc == 0 || fsc->firstPacketSent || std::difftime(std::time(0), fsc->ts) <= SECONDS_TO_WAIT_ANSWER)
				{
					++it;
					continue;
				}

				// if we have no more attempts
				if (fsc->resendCount >= ATTEMPTS_TO_SEND_FIRST_M)
				{
					// delete this download
					printSysMessage(fsc->endpoint, "sending ended with ERROR");
					_filesSent.erase(it++);
					delete fsc;
					continue;
				}

				// send again

				fsc->resendCount += 1;
				sendFirstFileMsg(fsc);
				++it;
			}
		}
	}
}

// async reading handler (udp socket)
void ChatClient::handleReceiveFrom(const boost::system::error_code& err, size_t size)
{
	if (err) return;

	// parse received packet
	MessageSys * pmsys = (MessageSys*)_data.data();
	if (pmsys->code <= msgLast)
	{
		boost::mutex::scoped_lock lk(_filesMutex);
		(_handlers[pmsys->code])->handle(_data.data(), size);
	}

	// wait for the next message
	_recvSocket.async_receive_from(
		boost::asio::buffer(_data), _recvEndpoint,
		boost::bind(&ChatClient::handleReceiveFrom, this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));
}

// try to understand user's input
void ChatClient::parseUserInput(const std::wstring& data)
{
	std::wstring tmp(data);

	// parse string
	if (tmp.size() > 1)
	{
		if (tmp.at(0) == L'*')
		{
			// no asterisks (system message have them)
			for (unsigned t = 0; t < tmp.size() && tmp[t] == L'*'; ++t);
		}
		else if (tmp.at(0) == L'@') {
			// Maybe, it is the private message (PM) such format:
			// @ip message
			std::string  ip;
			std::wstring msg;
			tmp.erase(0, 1);
			parseTwoStrings(tmp, ip, msg);
			if (ip.empty() || msg.empty())
			{
				throw std::logic_error("invalid format.");
			}
			else {
				sendMsg(parseEpFromString(ip), msg);
			}
			return;

		}
		else if (tmp.find(L"file ") == 0) {
			// Maybe, users wants to send the file:
			// file ip path
			tmp.erase(0, 5);
			std::string  ip;
			std::wstring path;
			parseTwoStrings(tmp, ip, path);
			if (ip.empty() || path.empty())
			{
				throw std::logic_error("invalid format.");
			}
			else {
				sendFile(parseEpFromString(ip), path);
			}
			return;
		}
	}

	if (tmp.empty()) return;

	// send on broadcast address
	sendMsg(_sendEndpoint, tmp);
}

// get endpoint from string

boost::asio::ip::udp::endpoint ChatClient::parseEpFromString(const std::string & ip)
{
	boost::system::error_code err;
	boost::asio::ip::address addr(boost::asio::ip::address::from_string(ip, err));
	if (err)
	{
		throw std::logic_error("can't parse this IP");
	}
	else {
		return boost::asio::ip::udp::endpoint(addr, _port);
	}
}

// print system message

void ChatClient::printSysMessage(
	const boost::asio::ip::udp::endpoint & endpoint
	, const std::string & msg)
{
	std::cout << endpoint.address().to_string() << " > " << "*** " << msg << " ***" << std::endl;
}

// split a string into two strings by first space-symbol

void ChatClient::parseTwoStrings(
	const std::wstring & tmp
	, std::string  & s1
	, std::wstring & s2)
{
	s1.clear();
	s2.clear();

	// try to find first space-symbol
	std::wstring::size_type t = tmp.find_first_of(L" \t");

	if (t == std::wstring::npos)
		return;

	s1 = std::string(tmp.begin(), tmp.begin() + t);
	s2 = tmp.substr(t + 1);
}

// read user's input

int ChatClient::loop()
{
	try
	{
		sendSysMsg(msgEnter);
		for (std::wstring line;;)
		{
			std::getline(std::wcin, line);
			// delete spaces to the right
			if (line.empty() == false)
			{
				std::wstring::iterator p;
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
			catch (const std::logic_error & e) {
				std::cout << "ERR: " << e.what() << std::endl;
			}
		}
		sendSysMsg(msgQuit);
	}
	catch (const std::exception & e) {
		std::cout << e.what() << std::endl;
	}

	return 0;
}

// system message

void ChatClient::sendSysMsg(unsigned sysMsg)
{
	boost::mutex::scoped_lock lk(_filesMutex);
	sendTo(_sendEndpoint, MessageBuilder::system(sysMsg));
}

// text message

void ChatClient::sendMsg(
	const boost::asio::ip::udp::endpoint & endpoint
	, const std::wstring & msg)
{
	boost::mutex::scoped_lock lk(_filesMutex);
	sendTo(endpoint, MessageBuilder::text(msg));
}

// send file message

void ChatClient::sendFile(
	const boost::asio::ip::udp::endpoint & endpoint
	, const std::wstring & path)
{
	boost::mutex::scoped_lock lk(_filesMutex);
	std::string fileName(path.begin(), path.end());

	std::ifstream input;
	input.open(fileName.c_str(), std::ifstream::in | std::ios_base::binary);
	if (!input.is_open())
	{
		std::cerr << "Error! Can't open file!" << std::endl;

		input.close();

		return;
	}
	input.close();

	// add to sent-files map
	FilesSentContext * fsc = new FilesSentContext;

	fsc->firstPacketSent = false;
	fsc->totalBlocks = 0;
	fsc->id = _fileId;
	fsc->path = fileName;
	fsc->endpoint = endpoint;
	fsc->endpoint.port(_port);
	fsc->resendCount = 0;
	fsc->ts = 0;
	_filesSent[_fileId] = fsc;
	_fileId += 1;

	sendFirstFileMsg(fsc);
}

// make and send block of file

void ChatClient::sendFirstFileMsg(FilesSentContext * ctx)
{
	std::string fileName(ctx->path.begin(), ctx->path.end());

	std::ifstream input;
	input.open(fileName.c_str(), std::ifstream::in | std::ios_base::binary);
	if (!input.is_open())
		throw std::logic_error("Can't open file!");

	// count quantity of blocks
	unsigned blocks = 0;
	input.seekg(0, std::ifstream::end);
	blocks = input.tellg() / FILE_BLOCK_MAX;
	if (input.tellg() % FILE_BLOCK_MAX)
		blocks += 1;

	input.close();
	ctx->totalBlocks = blocks;

	// cut path (send just name)

	std::string::size_type t = fileName.find_last_of('\\');
	if (t != std::string::npos) {
		fileName.erase(0, t + 1);
	}

	// make packet and send FMWFI

	sendTo(ctx->endpoint, MessageBuilder::fileBegin(ctx->id, ctx->totalBlocks, fileName));

	ctx->ts = std::time(0);
}

// make and send message with request of re-sending file block

void ChatClient::sendResendMsg(FileContext * ctx)
{
	sendTo(ctx->endpoint, MessageBuilder::resendFileBlock(
		ctx->id, ctx->blocksReceived));
}

void ChatClient::sendTo(
	const boost::asio::ip::udp::endpoint & e
	, const std::string & m)
{
	_sendSocket.send_to(boost::asio::buffer(m), e);
}