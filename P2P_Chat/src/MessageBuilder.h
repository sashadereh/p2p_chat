#ifndef MESSAGE_BUILDER_H
#define MESSAGE_BUILDER_H

#include "utils.h"

class MessageBuilder
{
public:
    static string system(unsigned char code);
    static string text(const wstring& msg);
};

#endif // MESSAGE_BUILDER_H
