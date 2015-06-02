#include "MessageBuilder.h"
#include "message_formats.h"
#include "utils.h"

std::string MessageBuilder::System(cc_string action, const string& peerId)
{
    string raw;
    size_t rawLen = strlen(action) + 1;
    raw.resize(rawLen + SZ_MESSAGE_SYS, 0);

    MessageSystem* msgSys = (MessageSystem*)raw.data();

    msgSys->_code = M_SYS;
    memcpy(msgSys->_action, action, rawLen);
    memcpy(msgSys->_peerId, peerId.c_str(), PEER_ID_SIZE + 1);

    return raw;
}

std::string MessageBuilder::PeerData(const wstring& nick, const string& id)
{
    string raw;
    size_t rawLen = nick.length() * sizeof(wchar_t);
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
    size_t rawLen = msg.length() * sizeof(wchar_t);
    raw.resize(rawLen + SZ_MESSAGE_TEXT, 0);

    MessageText* msgText = (MessageText*)raw.data();

    msgText->_code = M_TEXT;
    msgText->_length = msg.length();
    memcpy(msgText->_text, msg.data(), rawLen);
    memcpy(msgText->_peerId, peerId.c_str(), PEER_ID_SIZE + 1);

    return raw;
}

std::string MessageBuilder::FileBegin(uint32 id, uint32 totalBlocks, const string& name, const string& peerId)
{
    string raw;
    size_t rawLen = name.length() * sizeof(char);
    raw.resize(rawLen + SZ_MESSAGE_FILE_INFO, 0);

    MessageFileInfo* msgFileInfo = (MessageFileInfo*)raw.data();
    msgFileInfo->_code = M_FILE_BEGIN;
    msgFileInfo->_id = id;
    msgFileInfo->_totalBlocks = totalBlocks;
    msgFileInfo->_nameLength = name.length();
    memcpy(msgFileInfo->_name, name.data(), rawLen);
    memcpy(msgFileInfo->_peerId, peerId.c_str(), PEER_ID_SIZE + 1);

    return raw;
}

std::string MessageBuilder::FileBlock(uint32 id, uint32 block, cc_string data, uint32 size, const string& peerId)
{
    string raw;
    raw.resize(FILE_BLOCK_MAX + size, 0);
    MessageFileBlock* msgFileBlock = (MessageFileBlock*)raw.data();

    msgFileBlock->_code = M_FILE_BLOCK;
    msgFileBlock->_id = id;
    msgFileBlock->_block = block;
    msgFileBlock->_size = size;
    memcpy(msgFileBlock->_data, data, size);
    memcpy(msgFileBlock->_peerId, peerId.c_str(), PEER_ID_SIZE + 1);

    return raw;
}

std::string MessageBuilder::RequestForFileBlock(uint32 id, uint32 block, const string& peerId)
{
    string raw;
    raw.resize(SZ_MESSAGE_REQUEST_FOR_FILE_BLOCK, 0);
    MessageRequestForFileBlock* msgReqForFileBlock = (MessageRequestForFileBlock*)raw.data();

    msgReqForFileBlock->_code = M_REQ_FOR_FILE_BLOCK;
    msgReqForFileBlock->_id = id;
    msgReqForFileBlock->_block = block;
    memcpy(msgReqForFileBlock->_peerId, peerId.c_str(), PEER_ID_SIZE + 1);

    return raw;
}
