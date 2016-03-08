#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <cstring>
#include <memory>
#include <map>
#include <vector>
#include <fstream>
#include <ctime>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

#define MULTICAST_ADDR "239.255.0.1"

using namespace std;

typedef boost::asio::ip::udp::socket UdpSocket;
typedef boost::asio::ip::udp::endpoint UdpEndpoint;
typedef boost::asio::ip::address IpAddress;
typedef boost::asio::ip::address_v4 Ipv4Address;

typedef boost::thread Thread;

typedef boost::system::error_code ErrorCode;

typedef unsigned int uint;

static uint PORT = 54321;

#endif // UTILS_H
