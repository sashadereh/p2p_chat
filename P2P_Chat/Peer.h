#ifndef PEER_H
#define PEER_H

#include "utils.h"
#include "Logger.h"

class Peer
{
public:
    Peer(const wstring& nick, const string& peerId);
    Peer();
    ~Peer() { }

    bool WasPingSent() { return _pingSent; }

    void SetNickname(const wstring& nick);
    void SetIp(const string& ip);
    void SetLastActivityCheck(time_t time) { _lastActivityCheck = time; }
    void SetPingSentTime(time_t time) { _pingSentTime = time; }
    void SetPingSent(bool flag) { _pingSent = flag; }

    const wstring& GetNickname() const { return _nickname; }
    const string& GetId() const { return _id; }
    const string& GetIp() const { return _ip; }
    time_t GetLastActivityCheck() { return _lastActivityCheck; }
    time_t GetPingSentTime() { return _pingSentTime; }
private:
    wstring _nickname;
    string _ip;
    string _id;
    time_t _lastActivityCheck;
    time_t _pingSentTime;
    bool _pingSent;

    void GenerateId();
};

#endif // PEER_H
