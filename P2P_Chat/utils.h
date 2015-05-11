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

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/filesystem.hpp>

#define SIMULATE_PACKET_LOOSING 0

#define LOG_THREAD "LoggerThread"
#define SERVICE_THREAD "ServiceThread"
#define FILESWATCHER_THREAD "FilesThread"

#define PEER_ID_MAX_SIZE 20

using namespace std;

typedef boost::asio::ip::udp::socket UdpSocket;
typedef boost::asio::ip::udp::endpoint UdpEndpoint;
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

typedef unsigned int uint;

// Global variables
static uint PORT = 54321;
static bool DoShutdown = false;
static map<cc_string, auto_ptr<Thread>> ThreadsMap;

#endif // UTILS_H
