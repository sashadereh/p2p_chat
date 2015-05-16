#ifndef PEER_H
#define PEER_H

#include "utils.h"

class Peer
{
public:
    Peer(const wstring& nick);
    Peer() { GenerateId(); }
    ~Peer() {}
    void SetNickname(const wstring& nick);
    
    const wstring& GetNickname() const { return _nickname; }
    const string GetId() const { return string(_id); }
private:
    wstring _nickname;
    char _id[PEER_ID_SIZE + 1];
    time_t _lastAliveCheck;

    void GenerateId();
};

#endif // PEER_H
