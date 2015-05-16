#include "Peer.h"

Peer::Peer(const wstring& nick)
{
    SetNickname(nick);
    GenerateId();
}

void Peer::SetNickname(const wstring& nick)
{
    if (nick.empty())
        return;
    else
        _nickname = nick;
}

void Peer::GenerateId()
{
    const char extAlphabet[] = { "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" };
    size_t extAlphLen = strlen(extAlphabet);
    srand(unsigned int(time(0)));
    size_t i;
    for (i = 0; i < PEER_ID_SIZE; ++i)
    {
        _id[i] = extAlphabet[rand() % (extAlphLen - 1) + 0];
    }
    _id[i] = '\0';
}