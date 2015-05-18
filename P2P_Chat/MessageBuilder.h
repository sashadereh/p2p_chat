#ifndef MESSAGE_BUILDER_H
#define MESSAGE_BUILDER_H

#include "utils.h"

class MessageBuilder
{
public:
    static string system(uint8 code);
    static string PeerData(const wstring& nick, const string& id);
    static string text(const wstring& msg);
    static string fileBegin(uint32 id, uint32 totalBlocks, const string& name);
    static string fileBlock(uint32  id, uint32 block, cc_string data, uint32 size);
    static string resendFileBlock(uint32 id, uint32 block);
};

#endif // MESSAGE_BUILDER_H
