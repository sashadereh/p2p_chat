#include "MessageBuilder.h"
#include "message_formats.h"
#include "utils.h"

std::string MessageBuilder::System(cc_string action, const string& peerId)
{
    string raw;
    raw.resize(SZ_MESSAGE_SYS, 0);

    MessageSys* msgSys = (MessageSys*)raw.data();

    msgSys->_code = M_SYS;
    memcpy(msgSys, action, strlen(action) + 1);
    memcpy(msgSys->_peerId, peerId.c_str(), PEER_ID_SIZE + 1);

    return raw;
}

std::string MessageBuilder::PeerData(const wstring& nick, const string& id)
{
    string raw;
    int rawLen = nick.length() * sizeof(wchar_t);
    raw.resize(rawLen + SZ_MESSAGE_PEERDATA, 0);

    MessagePeerData* msgPeerData = (MessagePeerData*)raw.data();

    msgPeerData->_code = M_PEER_DATA;
    memcpy(msgPeerData->_id, id.c_str(), PEER_ID_SIZE + 1);
    msgPeerData->_nicknameLength = nick.length();
    memcpy(msgPeerData->_nickname, nick.data(), rawLen);

    return raw;
}

std::string MessageBuilder::Text(const wstring& msg, const string& peerId)
{
    string raw;
    int rawLen = msg.length() * sizeof(wchar_t);
    raw.resize(rawLen + SZ_MESSAGE_TEXT, 0);

    MessageText* msgText = (MessageText*)raw.data();

    msgText->code = M_TEXT;
    msgText->length = msg.length();
    memcpy(msgText->text, msg.data(), rawLen);
    memcpy(msgText->_peerId, peerId.c_str(), PEER_ID_SIZE + 1);

    return raw;
}

string MessageBuilder::FileBegin(uint32 id, uint32 totalBlocks, const string& fileName)
{
    string raw;
    int rawLen = fileName.length() * sizeof(char);
    raw.resize(rawLen + SZ_MESSAGE_FILE_BEGIN, 0);

    MessageFileBegin* pfb = (MessageFileBegin*)raw.data();
    pfb->code = M_FILE_BEGIN;
    pfb->id = id;
    pfb->totalBlocks = totalBlocks;
    pfb->nameLength = fileName.length();
    memcpy(pfb->name, fileName.data(), rawLen);

    return raw;
}

string MessageBuilder::FileBlock(uint32 id, uint32 block, cc_string data, uint32 size)
{
    string raw;
    raw.resize(FILE_BLOCK_MAX + size, 0);
    MessageFileBlock* pfp = (MessageFileBlock*)raw.data();

    pfp->code = M_FILE_BLOCK;
    pfp->id = id;
    pfp->block = block;
    pfp->size = size;
    memcpy(pfp->data, data, size);

    return raw;
}

string MessageBuilder::ResendableFileBlock(uint32 id, uint32 block)
{
    string raw;
    raw.resize(SZ_MESSAGE_RESEND_FILE_BLOCK, 0);
    MessageResendFileBlock* ms = (MessageResendFileBlock*)raw.data();

    ms->code = M_RESEND_FILE_BLOCK;
    ms->id = id;
    ms->block = block;

    return raw;
}
