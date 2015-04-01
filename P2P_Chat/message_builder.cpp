#include "message_builder.h"
#include "message_formats.h"
#include "utils.h"

string MessageBuilder::system(unsigned char code)
{
    string raw;
    raw.resize(SZ_MESSAGE_SYS, 0);

    MessageSys* ms = (MessageSys*)raw.data();
    ms->code = code;

    return raw;
}

string MessageBuilder::text(const wstring& msg)
{
    string raw;
    int rawLen = msg.length() * sizeof(wchar_t);
    raw.resize(rawLen + SZ_MESSAGE_TEXT, 0);

    MessageText* pmt = (MessageText*)raw.data();

    pmt->code = msgText;
    pmt->length = msg.length();
    memcpy(pmt->text, msg.data(), rawLen);

    return raw;
}

string MessageBuilder::fileBegin(uint id, uint totalBlocks, const string& fileName)
{
    string raw;
    int rawLen = fileName.length() * sizeof(char);
    raw.resize(rawLen + SZ_MESSAGE_FILE_BEGIN, 0);

    MessageFileBegin* pfb = (MessageFileBegin*)raw.data();
    pfb->code = msgFileBegin;
    pfb->id = id;
    pfb->totalBlocks = totalBlocks;
    pfb->nameLength = fileName.length();
    memcpy(pfb->name, fileName.data(), rawLen);

    return raw;
}

string MessageBuilder::fileBlock(uint id, uint block, const char* data, uint size)
{
    string raw;
    raw.resize(FILE_BLOCK_MAX + size, 0);
    MessageFileBlock* pfp = (MessageFileBlock*)raw.data();

    pfp->code = msgFileBlock;
    pfp->id = id;
    pfp->block = block;
    pfp->size = size;
    memcpy(pfp->data, data, size);

    return raw;
}

string MessageBuilder::resendFileBlock(uint id, uint block)
{
    string raw;
    raw.resize(SZ_MESSAGE_RESEND_FILE_BLOCK, 0);
    MessageResendFileBlock* ms = (MessageResendFileBlock*)raw.data();

    ms->code = msgResendFileBlock;
    ms->id = id;
    ms->block = block;

    return raw;
}