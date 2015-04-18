#include "ChatClient.h"
#include "MessageBuilder.h"
#include "message_formats.h"
#include "utils.h"

ChatClient* ChatClient::Handler::_chatClient = 0;

void ChatClient::handlerEnter::handle(const char*, size_t)
{
    _chatClient->printSysMessage(_chatClient->_recvEndpoint, "enter chat");
}

void ChatClient::handlerQuit::handle(const char*, size_t)
{
    _chatClient->printSysMessage(_chatClient->_recvEndpoint, "quit chat");
}

void ChatClient::handlerText::handle(const char* data, size_t)
{
    // output to the console
    MessageText* mt = (MessageText*)data;
    cout << _chatClient->_recvEndpoint.address().to_string() << " > ";
    wcout.write(mt->text, mt->length);
    cout << endl;
}

void ChatClient::handlerFileBegin::handle(const char* data, size_t)
{
    MessageFileBegin* mfb = (MessageFileBegin*)data;
    auto_ptr< UploadingFilesContext > ctx(new UploadingFilesContext);

    // maybe we have the same already (is downloading)

    stringstream ss;
    ss << _chatClient->_recvEndpoint.address().to_string() << ":" << mfb->id;
    string key(ss.str());

    UploadingFilesMap::iterator it = _chatClient->_files.find(key);
    if (it != _chatClient->_files.end())
    {
        ss.str(string());
        ss << "this file is already downloading '" << mfb->name << "'";
        _chatClient->printSysMessage(_chatClient->_recvEndpoint, ss.str());
        return;
    }

    // try to open

    if (mfb->nameLength > 256)
        mfb->nameLength = 256;

    mfb->name[mfb->nameLength] = '\0';

    ctx->fp.open(mfb->name, ios_base::out | ios_base::trunc | ios_base::binary);

    if (!ctx->fp.is_open())
    {
        ss.str(string());
        ss << "can't open for writing '" << mfb->name << "'";
        _chatClient->printSysMessage(_chatClient->_recvEndpoint, ss.str());
        return;
    }

    ss.str(string());
    ss << "start loading file '" << mfb->name << "'";
    _chatClient->printSysMessage(_chatClient->_recvEndpoint, ss.str());

    // fill in the fields
    ctx->endpoint = _chatClient->_recvEndpoint;
    ctx->endpoint.port(_chatClient->_port);
    ctx->id = mfb->id;
    ctx->blocks = mfb->totalBlocks;
    ctx->resendCount = 0;
    ctx->blocksReceived = 0;
    ctx->name = string(mfb->name, mfb->nameLength);
    ctx->ts = time(0);

    // requesting the first packet
    _chatClient->sendResendMsg(ctx.get());

    // save this for the next use
    _chatClient->_files[key] = ctx.release();
    return;
}

void ChatClient::handlerFileBlock::handle(const char *data, size_t)
{
    MessageFileBlock* mfp = (MessageFileBlock*)data;

    stringstream ss;
    ss << _chatClient->_recvEndpoint.address().to_string() << ":" << mfp->id;

    string key(ss.str());
    UploadingFilesContext* ctx = 0;
    UploadingFilesMap::iterator it = _chatClient->_files.find(key);
    if (it != _chatClient->_files.end())
        ctx = _chatClient->_files[key];

    if (ctx == 0 || mfp->block >= ctx->blocks)
    {
        _chatClient->printSysMessage(_chatClient->_recvEndpoint, "received block is out of queue!");
        return;
    }

    ctx->fp.write(mfp->data, mfp->size);
    ctx->resendCount = 0;
    ctx->blocksReceived += 1;
    ctx->ts = time(0);

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
        ss.str(string());
        ss << "downloaded block " << mfp->block << " from " << ctx->blocks;
        _chatClient->printSysMessage(_chatClient->_recvEndpoint, ss.str());
    }

    // requesting the next block

    _chatClient->sendTo(ctx->endpoint, MessageBuilder::resendFileBlock(ctx->id, ctx->blocksReceived));

    return;
}

void ChatClient::handlerResendFileBlock::handle(const char *data, size_t)
{
    MessageResendFileBlock* mrfp = (MessageResendFileBlock*)data;

    if (SIMULATE_PACKET_LOOSING == 1)
    {
        srand((uint)time(NULL));
        if ((rand() % 10 + 1) > 7)
            return;
    }

    if (_chatClient->_filesSent.find(mrfp->id) == _chatClient->_filesSent.end())
        return;

    SentFilesContext * fsc = _chatClient->_filesSent[mrfp->id];
    if (mrfp->block >= fsc->totalBlocks)
        return;

    // open file

    ifstream input;
    input.open(fsc->path.c_str(), ifstream::in | ios_base::binary);
    if (!input.is_open())
    {
        _chatClient->printSysMessage(_chatClient->_recvEndpoint, "can't open file.");
        return;
    }

    // read and send needed block

    char raw[FILE_BLOCK_MAX];
    input.seekg(mrfp->block * FILE_BLOCK_MAX);
    input.read(raw, sizeof(raw));

    _chatClient->sendTo(fsc->endpoint, MessageBuilder::fileBlock(mrfp->id, mrfp->block, raw, (uint)input.gcount()));

    input.close();

    if (mrfp->block == 0)
        fsc->firstBlockSent = true;
}