#include "MessageBuilder.h"
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

    pmt->code = MT_TEXT;
    pmt->length = msg.length();
    memcpy(pmt->text, msg.data(), rawLen);

    return raw;
}