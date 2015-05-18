#ifndef PEER_H
#define PEER_H

#include "utils.h"

class Peer
{
public:
    Peer(const wstring& nick, cc_string peerId = NULL);
    Peer() { GenerateId(); }
    ~Peer() {}
    void SetNickname(const wstring& nick);
    void SetIp(const string& ip);
    
    const wstring& GetNickname() const { return _nickname; }
    const string GetId() const { return string(_id); }
private:
    wstring _nickname;
    string _ip;
    char _id[PEER_ID_SIZE + 1];
    time_t _lastAliveCheck;

    void GenerateId();
};

#endif // PEER_H
