#ifndef PEER_H
#define PEER_H

#include "utils.h"

class Peer
{
public:
    Peer(const wstring& nick, const string& peerId);
    Peer(bool makeHandshake = true);
    ~Peer() {}
    void SetNickname(const wstring& nick);
    void SetIp(const string& ip);
    void SetAliveCheck(time_t time);
    bool WasHandshake() { return _handshake; }
    void MakeHandshake() { _handshake = true; }

    const wstring& GetNickname() const { return _nickname; }
    const string& GetId() const { return _id; }
    const string& GetIp() const { return _ip; }
    time_t GetLastAliveCheck() { return _lastAliveCheck; }
private:
    wstring _nickname;
    string _ip;
    string _id;
    time_t _lastAliveCheck;
    bool _handshake;

    void GenerateId();
};

#endif // PEER_H
