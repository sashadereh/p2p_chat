#ifndef MESSAGE_FORMATS_H
#define MESSAGE_FORMATS_H

#include "utils.h"

enum MessageType
{
    M_SYS,
    M_TEXT,
    M_FILE_BEGIN,
    M_FILE_BLOCK,
    M_REQ_FOR_FILE_BLOCK,
    M_PEER_DATA,
    FIRST = M_SYS,
    LAST = M_PEER_DATA
};

/*
Compiler can "align" fields of the structure. Yes, optimization.
We don't allow "him" to do it.
Compiler make this in order to optimize the speed of access to struct's fields. (Multiple of a pow(2))

For example:
    struct MyStruct
    {
        uint8 code; // 1 byte
        uint32 length; // 4 bytes
    };
    Compiler can "insert" 3-nil bytes between code and length in order the size of length will be the multiple of a pow(2).
*/
#pragma pack(push, 1)

// System message
struct MessageSystem
{
    uint8 _code;
    char _peerId[PEER_ID_SIZE + 1];
    char _action[1];
};

#define SZ_MESSAGE_SYS (sizeof(uint8) + sizeof(char[PEER_ID_SIZE + 1]))

struct MessagePeerData
{
    uint8 _code;
    uint32 _nicknameLength;
    char _id[PEER_ID_SIZE + 1];
    wchar_t _nickname[1];
};

#define SZ_MESSAGE_PEERDATA (sizeof(uint8) + sizeof(uint32) + sizeof(char[PEER_ID_SIZE + 1]))

// Text message
struct MessageText
{
    uint8 _code;
    uint32 _length;
    char _peerId[PEER_ID_SIZE + 1];
    // "open array", we don't know about size of data. This field will have address in struct.
    wchar_t _text[1];
};

#define SZ_MESSAGE_TEXT (sizeof(uint8) + sizeof(uint32) + sizeof(char[PEER_ID_SIZE + 1]))

// File information message (First message, which file-sender send, when sending file)
struct MessageFileInfo
{
    uint8 _code;
    uint32 _id;
    uint32 _totalBlocks;
    uint32 _nameLength;
    char _peerId[PEER_ID_SIZE + 1];
    char _name[1];
};

#define SZ_MESSAGE_FILE_INFO (sizeof(uint8) + 3 * sizeof(uint32) + sizeof(char[PEER_ID_SIZE + 1]))

// File block message
struct MessageFileBlock
{
    uint8 _code;
    uint32 _id;
    uint32 _block;
    uint32 _size;
    char _peerId[PEER_ID_SIZE + 1];
    char _data[1];
};

#define SZ_MESSAGE_FILE_BLOCK (sizeof(uint8) + 3 * sizeof(uint32) + sizeof(char[PEER_ID_SIZE + 1]))

#define FILE_BLOCK_MAX (6 * 1024)

// File block message, which is re-sent
struct MessageRequestForFileBlock
{
    uint8 _code;
    uint32 _id;
    uint32 _block;
    char _peerId[PEER_ID_SIZE + 1];
};

#define SZ_MESSAGE_REQUEST_FOR_FILE_BLOCK (sizeof(uint8) + 2 * sizeof(uint32) + sizeof(char[PEER_ID_SIZE + 1]))

#pragma pack(pop)

#endif // MESSAGE_FORMATS_H
