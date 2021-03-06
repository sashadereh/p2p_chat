#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <string>
#include <sstream>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <memory>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>

#ifdef __linux__
#include <boost/locale/encoding_utf.hpp>
#elif _WIN32
#include <codecvt>
#endif

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/filesystem.hpp>

#define SIMULATE_PACKET_LOOSING 1

#define LOG_THREAD "LoggerThread"
#define BOOST_SERVICE_THREAD "BoostServiceThread"
#define FILESWATCHER_THREAD "FilesThread"

#define PEER_ID_SIZE 20

using namespace std;

typedef boost::asio::ip::udp::socket UdpSocket;
typedef boost::asio::ip::udp::endpoint UdpEndpoint;
typedef boost::asio::ip::udp::resolver UdpResolver;
typedef boost::asio::ip::address IpAddress;
typedef boost::asio::ip::address_v4 Ipv4Address;

typedef boost::thread Thread;
typedef boost::mutex Mutex;
typedef boost::mutex::scoped_lock ScopedLock;

typedef boost::system::error_code ErrorCode;

typedef ofstream OutFile;
typedef ifstream InFile;

typedef char* c_string;
typedef const char* cc_string;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned long uint32;
typedef unsigned long uint64;

// Global variables
static uint16 Port = 54321;
static bool DoShutdown = false;
static map<string, auto_ptr<Thread>> ThreadsMap;

bool IsIpV4(const string& ip);
string to_string(const wstring& wstr);

#endif // UTILS_H
