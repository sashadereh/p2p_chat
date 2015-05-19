#ifndef PEER_H
#define PEER_H

#include "utils.h"

class Peer
{
public:
    Peer(const wstring& nick, const string& peerId);
    Peer();
    ~Peer() {}
    void SetNickname(const wstring& nick);
    void SetIp(const string& ip);
    void SetAliveCheck(time_t time);
    
    const wstring& GetNickname() const { return _nickname; }
    const string& GetId() const { return _id; }
    const string& GetIp() const { return _ip; }
private:
    wstring _nickname;
    string _ip;
    string _id;
    time_t _lastAliveCheck;

    void GenerateId();
};

#endif // PEER_H
