#include "MessageBuilder.h"
#include "message_formats.h"
#include "utils.h"

string MessageBuilder::system(uint8 code)
{
    string raw;
    raw.resize(SZ_MESSAGE_SYS, 0);

    MessageSys* ms = (MessageSys*)raw.data();
    ms->_code = code;

    return raw;
}

std::string MessageBuilder::PeerData(const wstring& nick, const string& id)
{
    string raw;
    int rawLen = nick.length() * sizeof(wchar_t);
    raw.resize(rawLen + SZ_MESSAGE_PEERDATA, 0);

    MessagePeerData* messPeerData = (MessagePeerData*)raw.data();

    messPeerData->_code = M_PEER_DATA;
    memcpy(messPeerData->_id, id.c_str(), PEER_ID_SIZE + 1);
    messPeerData->_nicknameLength = nick.length();
    memcpy(messPeerData->_nickname, nick.data(), rawLen);

    return raw;
}

string MessageBuilder::text(const wstring& msg)
{
    string raw;
    int rawLen = msg.length() * sizeof(wchar_t);
    raw.resize(rawLen + SZ_MESSAGE_TEXT, 0);

    MessageText* pmt = (MessageText*)raw.data();

    pmt->code = M_TEXT;
    pmt->length = msg.length();
    memcpy(pmt->text, msg.data(), rawLen);

    return raw;
}

string MessageBuilder::fileBegin(uint32 id, uint32 totalBlocks, const string& fileName)
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

string MessageBuilder::fileBlock(uint32 id, uint32 block, cc_string data, uint32 size)
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

string MessageBuilder::resendFileBlock(uint32 id, uint32 block)
{
    string raw;
    raw.resize(SZ_MESSAGE_RESEND_FILE_BLOCK, 0);
    MessageResendFileBlock* ms = (MessageResendFileBlock*)raw.data();

    ms->code = M_RESEND_FILE_BLOCK;
    ms->id = id;
    ms->block = block;

    return raw;
}
