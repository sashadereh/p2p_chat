#ifndef MESSAGE_FORMATS_H
#define MESSAGE_FORMATS_H

#include "utils.h"

enum MessageType
{
    M_SYS,
    M_TEXT,
    M_FILE_BEGIN,
    M_FILE_BLOCK,
    M_RESEND_FILE_BLOCK,
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
struct MessageSys
{
    uint8 _code;
    char _peerId[PEER_ID_SIZE + 1];
    char _action[1];
};

#define SZ_MESSAGE_SYS (sizeof(uint8))

struct MessagePeerData
{
    uint8 _code;
    uint32 _nicknameLength;
    char _id[PEER_ID_SIZE + 1];
    wchar_t _nickname[1];
};

#define SZ_MESSAGE_PEERDATA (SZ_MESSAGE_SYS + sizeof(uint) + sizeof(char[PEER_ID_SIZE + 1]))

// Text message
struct MessageText
{
    uint8 code;
    uint32 length;
    // "open array", we don't know about size of data. This field will have address in struct.
    wchar_t text[1];
};

#define SZ_MESSAGE_TEXT (SZ_MESSAGE_SYS + sizeof(uint))

// File information message (First message, which file-sender send, when sending file)
struct MessageFileBegin
{
    uint8 code;
    uint32 id;
    uint32 totalBlocks;
    uint32 nameLength;
    char name[1];
};

#define SZ_MESSAGE_FILE_BEGIN (SZ_MESSAGE_SYS + 3 * sizeof(uint))

// File block message
struct MessageFileBlock
{
    uint8 code;
    uint32 id;
    uint32 block;
    uint32 size;
    char data[1];
};

#define SZ_MESSAGE_FILE_BLOCK (SZ_MESSAGE_SYS + 3 * sizeof(uint))

#define FILE_BLOCK_MAX (6 * 1024)

// File block message, which is re-sent
struct MessageResendFileBlock
{
    uint8 code;
    uint32 id;
    uint32 block;
};

#define SZ_MESSAGE_RESEND_FILE_BLOCK (SZ_MESSAGE_SYS + 2 * sizeof(uint))

#pragma pack(pop)

#endif // MESSAGE_FORMATS_H
