#ifndef HANDLERS
#define HANDLERS

#include "utils.h"
#include "ChatClient.h"

class Handler
{
public:
    virtual ~Handler() {}
    virtual void handle(cc_stringdata, size_t size) = 0;
    static void setChatInstance(ChatClient* chatClient) { _chatClient = chatClient; }
protected:
    static ChatClient* _chatClient;
};
typedef vector< Handler* > Handlers;

class HandlerSys : public Handler {
public:
    void handle(cc_string data, size_t size);
};

class handlerText : public Handler {
public:
    void handle(cc_string data, size_t size);
};

class HandlerPeerData : public Handler {
public:
    void handle(cc_string data, size_t size);
};

class handlerFileBegin : public Handler {
public:
    void handle(cc_string data, size_t size);
};

class handlerFileBlock : public Handler {
public:
    void handle(cc_string data, size_t size);
};

class handlerResendFileBlock : public Handler {
public:
    void handle(cc_string data, size_t size);
};

#endif