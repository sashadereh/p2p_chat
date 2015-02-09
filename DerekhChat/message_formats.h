#ifndef MESSAGE_FORMATS_H
#define MESSAGE_FORMATS_H

enum MessageType
{
	msgEnter,
	msgQuit,
	msgText,
	msgFileBegin,
	msgFileBlock,
	msgResendFileBlock,
	msgFirst = msgEnter,
	msgLast = msgResendFileBlock
};

/*
Compiler can "align" fields of the structure. Yes, optimization.
We don't allow "him" to do it.
Compiler make this in order to optimize the speed of access to struct's fields. (Multiple of a pow(2))

For example:
	struct MyStruct
	{
		unsigned char code; // 1 byte
		unsigned int length; // 4 bytes
	};
	Compiler can "insert" 3-nil bytes between code and length in order the size of length will be the multiple of a pow(2).
*/
#pragma pack(push, 1)

// System message
struct MessageSys
{
	unsigned char code;
};

#define SZ_MESSAGE_SYS (sizeof(unsigned char))

// Text message
struct MessageText
{
	unsigned char code;
	unsigned int  length;
	// "open array", we don't know about size of data. This field will have address in struct.
	wchar_t       text[1];
};

#define SZ_MESSAGE_TEXT (SZ_MESSAGE_SYS + sizeof(unsigned int))

// File information message (First message, which file-sender send, when sending file)
struct MessageFileBegin
{
	unsigned char code;
	unsigned int  id;
	unsigned int  totalBlocks;
	unsigned int  nameLength;
	char          name[1];
};

#define SZ_MESSAGE_FILE_BEGIN (SZ_MESSAGE_SYS + 3 * sizeof(unsigned int))

// File block message
struct MessageFileBlock
{
	unsigned char code;
	unsigned int  id;
	unsigned int  block;
	unsigned int  size;
	char          data[1];
};

#define SZ_MESSAGE_FILE_BLOCK (SZ_MESSAGE_SYS + 3 * sizeof(unsigned int))

#define FILE_BLOCK_MAX (6 * 1024)

// File block message, which is re-sent
struct MessageResendFileBlock
{
	unsigned char code;
	unsigned int  id;
	unsigned int  block;
};

#define SZ_MESSAGE_RESEND_FILE_BLOCK (SZ_MESSAGE_SYS + 2 * sizeof(unsigned int))

#pragma pack(pop)

#endif // MESSAGE_FORMATS_H