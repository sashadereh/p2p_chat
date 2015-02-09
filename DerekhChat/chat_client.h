#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <fstream>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

#define SIMULATE_PACKET_LOOSING 0 // Simulate loosing packets :)

class ChatClient
{
public:
	ChatClient(int port);
	~ChatClient();

	int loop();

private:
	// downloading files

	struct FileContext
	{
		boost::asio::ip::udp::endpoint endpoint; // from
		unsigned int  blocks;             // total blocks
		unsigned int  blocksReceived;     // received blocks
		unsigned int  resendCount;        // sending requests (for one block!)
		unsigned int  id;                 // file id on the receiver side
		std::ofstream fp;                 // read from it
		time_t        ts;                 // last block received
		std::string   name;               // file name
	};
	typedef std::map< std::string, FileContext* > FilesMap;

	// sent files

	struct FilesSentContext
	{
		boost::asio::ip::udp::endpoint endpoint; // to
		unsigned int id;					// file id on the sender side
		unsigned int totalBlocks;			// total blocks
		std::string	 path;					// file path
		bool		firstPacketSent;		// has first block been sent?
		time_t       ts;					// first block sent
		unsigned int resendCount;			// sending requests (for one block!)
	};
	typedef std::map< unsigned, FilesSentContext* > FilesSentMap;

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
	typedef std::vector< Handler* > Handlers;

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

	boost::asio::io_service        _ioService;
	boost::asio::ip::udp::socket   _sendSocket;
	boost::asio::ip::udp::socket   _recvSocket;
	boost::asio::ip::udp::endpoint _sendEndpoint;
	boost::asio::ip::udp::endpoint _recvEndpoint;
	boost::array<char, 64 * 1024>  _data;
	std::auto_ptr<boost::thread>   _serviceThread;
	std::auto_ptr<boost::thread>   _watcherThread;
	volatile int                   _threadsRunned;
	int                            _port;
	unsigned int                   _fileId;
	FilesMap                       _files;
	FilesSentMap                   _filesSent;
	boost::mutex                   _filesMutex;
	Handlers                       _handlers;

	// async

	void serviceThread();
	void serviceFilesWatcher();
	void handleReceiveFrom(const boost::system::error_code& error, size_t bytes_recvd);

	// parsing

	boost::asio::ip::udp::endpoint parseEpFromString(const std::string&);
	void parseUserInput(const std::wstring& data);
	void parseTwoStrings(const std::wstring& str, std::string& s1, std::wstring& s2);
	void printSysMessage(const boost::asio::ip::udp::endpoint& endpoint, const std::string& msg);

	// senders

	void sendSysMsg(unsigned sysMsg);
	void sendMsg(const boost::asio::ip::udp::endpoint& endpoint, const std::wstring& message);
	void sendFile(const boost::asio::ip::udp::endpoint& endpoint, const std::wstring& path);
	void sendTo(const boost::asio::ip::udp::endpoint& endpoint, const std::string& m);
	void sendResendMsg(FileContext* ctx);
	void sendFirstFileMsg(FilesSentContext* ctx);
};

#endif // CHAT_CLIENT_H
