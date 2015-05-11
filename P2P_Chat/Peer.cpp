#include "Peer.h"

Peer::Peer(const wstring& nick)
{
    SetNickname(nick);
    srand(time(0));
}

void Peer::SetNickname(const wstring& nick)
{
    if (nick.empty())
        return;
    else
        _nickname = nick;
}

void Peer::SetId()
{
    const char extAlphabet[] = { "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" };
    size_t extAlphLen = strlen(extAlphabet);
    srand(unsigned int(time(0)));
    for (size_t i = 0; i < PEER_ID_MAX_SIZE; ++i)
    {
        _id[i] = extAlphabet[rand() % (extAlphLen - 1) + 0];
    }
}