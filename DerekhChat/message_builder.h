#ifndef MESSAGE_BUILDER_H
#define MESSAGE_BUILDER_H

#include <string>

class MessageBuilder
{
public:
	static std::string system(unsigned char code);
	static std::string text(const std::wstring& msg);
	static std::string fileBegin(unsigned int id, unsigned int totalBlocks, const std::string& name);
	static std::string fileBlock(unsigned int  id, unsigned int block, const char* data, unsigned int size);
	static std::string resendFileBlock(unsigned int id, unsigned int block);
};

#endif // MESSAGE_BUILDER_H
