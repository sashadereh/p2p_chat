#ifndef MESSAGE_BUILDER_H
#define MESSAGE_BUILDER_H

#include "utils.h"

class MessageBuilder
{
public:
    static string system(unsigned char code);
    static string PeerData(const wstring& nick, const string& id);
    static string text(const wstring& msg);
    static string fileBegin(uint id, uint totalBlocks, const string& name);
    static string fileBlock(uint  id, uint block, const char* data, uint size);
    static string resendFileBlock(uint id, uint block);
};

#endif // MESSAGE_BUILDER_H
