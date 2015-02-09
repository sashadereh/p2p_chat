#include <ctime>

#include "chat_client.h"
#include "message_builder.h"
#include "message_formats.h"

ChatClient* ChatClient::Handler::_chatClient = 0;

void ChatClient::handlerEnter::handle(const char*, size_t) {
	_chatClient->printSysMessage(_chatClient->_recvEndpoint, "enter chat");
}

void ChatClient::handlerQuit::handle(const char*, size_t) {
	_chatClient->printSysMessage(_chatClient->_recvEndpoint, "quit chat");
}

void ChatClient::handlerText::handle(const char* data, size_t) {
	// output to the console
	MessageText* mt = (MessageText*)data;
	std::cout << _chatClient->_recvEndpoint.address().to_string() << " > ";
	std::wcout.write(mt->text, mt->length);
	std::cout << std::endl;
}

void ChatClient::handlerFileBegin::handle(const char* data, size_t) {
	MessageFileBegin* mfb = (MessageFileBegin*)data;
	std::auto_ptr< FileContext > ctx(new FileContext);

	// maybe we have the same already (is downloading)

	std::stringstream ss;
	ss << _chatClient->_recvEndpoint.address().to_string() << ":" << mfb->id;
	std::string key(ss.str());

	FilesMap::iterator it = _chatClient->_files.find(key);
	if (it != _chatClient->_files.end())
	{
		ss.str(std::string());
		ss << "this file is already downloading '" << mfb->name << "'";
		_chatClient->printSysMessage(_chatClient->_recvEndpoint, ss.str());
		return;
	}

	// try to open

	if (mfb->nameLength > 256)
		mfb->nameLength = 256;

	mfb->name[mfb->nameLength] = '\0';

	ctx->fp.open(mfb->name, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);

	if (!ctx->fp.is_open())
	{
		ss.str(std::string());
		ss << "can't open for writing '" << mfb->name << "'";
		_chatClient->printSysMessage(_chatClient->_recvEndpoint, ss.str());
		return;
	}

	ss.str(std::string());
	ss << "start loading file '" << mfb->name << "'";
	_chatClient->printSysMessage(_chatClient->_recvEndpoint, ss.str());

	// fill in the fields
	ctx->endpoint = _chatClient->_recvEndpoint;
	ctx->endpoint.port(_chatClient->_port);
	ctx->id = mfb->id;
	ctx->blocks = mfb->totalBlocks;
	ctx->resendCount = 0;
	ctx->blocksReceived = 0;
	ctx->name = std::string(mfb->name, mfb->nameLength);
	ctx->ts = std::time(0);

	// requesting the first packet
	_chatClient->sendResendMsg(ctx.get());

	// save this for the next use
	_chatClient->_files[key] = ctx.release();
	return;
}

void ChatClient::handlerFileBlock::handle(const char *data, size_t) {
	MessageFileBlock * mfp = (MessageFileBlock*)data;

	std::stringstream ss;
	ss << _chatClient->_recvEndpoint.address().to_string() << ":" << mfp->id;

	std::string key(ss.str());
	FileContext * ctx = 0;
	FilesMap::iterator it = _chatClient->_files.find(key);
	if (it != _chatClient->_files.end())
		ctx = _chatClient->_files[key];

	if (ctx == 0 || mfp->block >= ctx->blocks)
	{
		_chatClient->printSysMessage(_chatClient->_recvEndpoint, "recived block out of queue!");
		return;
	}

	ctx->fp.write(mfp->data, mfp->size);
	ctx->resendCount = 0;
	ctx->blocksReceived += 1;
	ctx->ts = std::time(0);

	// we have all blocks?

	if (ctx->blocks == ctx->blocksReceived)
	{
		_chatClient->printSysMessage(ctx->endpoint, "done loading file");
		_chatClient->_files.erase(_chatClient->_files.find(key));
		ctx->fp.close();
		delete ctx;
		return;
	}

	if (!(mfp->block % 89))
	{
		ss.str(std::string());
		ss << "downloaded block " << mfp->block << " from " << ctx->blocks;
		_chatClient->printSysMessage(_chatClient->_recvEndpoint, ss.str());
	}

	// requesting the next block

	_chatClient->sendTo(ctx->endpoint, MessageBuilder::resendFileBlock(ctx->id, ctx->blocksReceived));

	return;
}

void ChatClient::handlerResendFileBlock::handle(const char *data, size_t) {
	MessageResendFileBlock * mrfp = (MessageResendFileBlock*)data;

	// SIMULATING PACKET LOOSING! (we won't send requested packet in 30% cases)

	if (SIMULATE_PACKET_LOOSING == 1)
	{
		srand(time(NULL));
		if ((rand() % 10 + 1) > 7)
			return;
	}

	if (_chatClient->_filesSent.find(mrfp->id) == _chatClient->_filesSent.end())
		return;

	FilesSentContext * fsc = _chatClient->_filesSent[mrfp->id];
	if (mrfp->block >= fsc->totalBlocks)
		return;

	// open file

	std::ifstream input;
	input.open(fsc->path.c_str(), std::ifstream::in | std::ios_base::binary);
	if (!input.is_open())
	{
		_chatClient->printSysMessage(_chatClient->_recvEndpoint, "can't open file.");
		return;
	}

	// read and send needen block

	char raw[FILE_BLOCK_MAX];
	input.seekg(mrfp->block * FILE_BLOCK_MAX);
	input.read(raw, sizeof(raw));

	_chatClient->sendTo(fsc->endpoint, MessageBuilder::fileBlock(mrfp->id, mrfp->block, raw, input.gcount()));

	input.close();

	if (mrfp->block == 0)
		fsc->firstPacketSent = true;
}