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
    
    bool WasHandshake() { return _handshake; }
    bool IsPingSent() { return _pingSent; }
    bool IsPongReceived() { return _pongReceived; }
    bool NeedSendPong() { return _sendPong; }
    

    void SetAliveCheck(time_t time) { _lastAliveCheck = time; }
    void SetPingSentTime(time_t time) { _sentPing = time; }
    void SetPingSent(bool flag) { _pingSent = flag; }
    void ShouldSendPong(bool flag) { _sendPong = flag; }
    void SetPongReceived(bool flag) { _pongReceived = flag; }
    void MakeHandshake() { _handshake = true; }

    const wstring& GetNickname() const { return _nickname; }
    const string& GetId() const { return _id; }
    const string& GetIp() const { return _ip; }
    time_t GetLastAliveCheck() { return _lastAliveCheck; }
    time_t GetPingSentTime() { return _sentPing; }
private:
    wstring _nickname;
    string _ip;
    string _id;
    time_t _lastAliveCheck;
    time_t _sentPing;
    bool _handshake;
    bool _pingSent;
    bool _sendPong;
    bool _pongReceived;

    void GenerateId();
};

#endif // PEER_H
