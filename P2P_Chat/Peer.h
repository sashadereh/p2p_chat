#ifndef PEER_H
#define PEER_H

#include "utils.h"

class Peer
{
public:
    Peer(const wstring& nick);
    ~Peer() {}
    void SetNickname(const wstring& nick);
private:
    wstring _nickname;
    char _id[PEER_ID_MAX_SIZE];
    time_t _lastAliveCheck;

    void SetId();
};

#endif // PEER_H
