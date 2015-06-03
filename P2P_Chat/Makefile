PRODUCT_NAME := P2P_Chat
CXXFLAGS := -std=c++11
FS_LIB := ${BOOST_LIB}/libboost_filesystem.a
THREAD_LIB := ${BOOST_LIB}/libboost_thread.a

ChatClient.o : ChatClient.cpp ChatClient.h Logger.h MessageBuilder.h message_formats.h utils.h
		c++ ${CXXFLAGS} -I ${BOOST_ROOT} ChatClient.cpp \
	${THREAD_LIB} \
	${FS_LIB}

Logger.o : Logger.cpp Logger.h utils.h
		c++ ${CXXFLAGS} -I ${BOOST_ROOT} Logger.cpp \
	${THREAD_LIB}

MessageBuilder.o : MessageBuilder.cpp MessageBuilder.h message_formats.h utils.h
		c++ ${CXXFLAGS} -I ${BOOST_ROOT} MessageBuilder.cpp

Peer.o : Peer.cpp Peer.h Logger.h utils.h
		c++ ${CXXFLAGS} -I $BOOST_ROOT Peer.cpp

utils.o : utils.cpp utils.h
		c++ ${CXXFLAGS} -I ${BOOST_ROOT} utils.cpp

handlers.o : handlers.cpp ChatClient.h Logger.h MessageBuilder.h message_formats.h utils.h
		c++ ${CXXFLAGS} -I ${BOOST_ROOT} handlers.cpp

main.o : main.cpp ChatClient.h Logger.h utils.h
		c++ ${CXXFLAGS} -I ${BOOST_ROOT} main.cpp

main: main.o handlers.o utils.o Peer.o MessageBuilder.o Logger.o ChatClient.o
		c++ ${CXXFLAGS} ChatClient.o Logger.o MessageBuilder.o Peer.o utils.o handlers.o main.o -o ${PRODUCT_NAME}