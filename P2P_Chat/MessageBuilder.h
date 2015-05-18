#ifndef MESSAGE_BUILDER_H
#define MESSAGE_BUILDER_H

#include "utils.h"

class MessageBuilder
{
public:
    static string System(cc_string action, const string& peerId);
    static string PeerData(const wstring& nick, const string& id);
    static string Text(const wstring& msg, const string& peerId);
    static string FileBegin(uint32 id, uint32 totalBlocks, const string& name);
    static string FileBlock(uint32  id, uint32 block, cc_string data, uint32 size);
    static string ResendableFileBlock(uint32 id, uint32 block);
};

#endif // MESSAGE_BUILDER_H
