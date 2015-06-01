#include "Peer.h"

Peer::Peer(const wstring& nick, const string& peerId) : _pingSent(false)
{    
    SetNickname(nick);
    if (!peerId.empty())
        _id = peerId;
    else
        GenerateId();
    SetLastActivityCheck(time(0));
}

Peer::Peer() : _pingSent(false)
{
    GenerateId();
    SetLastActivityCheck(time(0));
}


void Peer::SetNickname(const wstring& nick)
{
    if (nick.empty())
        return;
    else
        _nickname = nick;
}

void Peer::SetIp(const string& ip)
{
    if (ip.empty())
        return;
    else
        _ip = ip;
}

void Peer::GenerateId()
{
    char id[PEER_ID_SIZE + 1];
    const char extAlphabet[] = { "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" };
    size_t extAlphLen = strlen(extAlphabet);
    srand(unsigned int(time(0)));

    size_t i;
    for (i = 0; i < PEER_ID_SIZE; ++i)
    {
        id[i] = extAlphabet[rand() % (extAlphLen - 1) + 0];
    }
    id[i] = '\0';

    _id.assign(id);

}
