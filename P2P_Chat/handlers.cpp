#include "ChatClient.h"
#include "MessageBuilder.h"
#include "message_formats.h"
#include "Logger.h"

ChatClient* ChatClient::Handler::_chatClient = 0;

void ChatClient::HandlerSys::handle(cc_string data, size_t size)
{
    MessageSystem* msgSys = (MessageSystem*)data;

    string action;
    action.assign(msgSys->_action);
    string peerId;
    peerId.assign(msgSys->_peerId);

    map<cc_string, Peer>::iterator it = _chatClient->_peersMap.find(peerId.c_str());

    if (action == "quit")
    {
        if (peerId == _chatClient->_thisPeer.GetId())
        {
            return;
        }
        else if (it == _chatClient->_peersMap.end())
        {
            Logger::GetInstance()->Trace("Received 'quit' message from unknown peer ", peerId);
        }
        else
        {
            wcout << it->second.GetNickname();
            cout << " left out chat." << endl;
            Logger::GetInstance()->Trace("Peer ", it->second.GetId(), " is offline. Removing from peers map");
            _chatClient->_peersMap.erase(it->first);
        }
    }
    else if (action == "ping")
    {
        
    }
    else if (action == "pong")
    {
       
    }
}

void ChatClient::HandlerText::handle(cc_string data, size_t size)
{
    MessageText* msgText = (MessageText*)data;

    string peerId;
    peerId.assign(msgText->_peerId);

    map<cc_string, Peer>::iterator it = _chatClient->_peersMap.find(peerId.c_str());

    if (peerId == _chatClient->_thisPeer.GetId())
    {
        wcout << endl << _chatClient->_thisPeer.GetNickname() << " > ";
    }
    else if (it == _chatClient->_peersMap.end())
    {
        Logger::GetInstance()->Trace("Received text message from unknown peer ", peerId);
        return;
    }
    else
    {
        it->second.SetLastActivityCheck(time(0));
        wcout << endl << it->second.GetNickname() << " > ";
    }
    wcout.write(msgText->_text, msgText->_length);
    wcout << endl;
}

void ChatClient::HandlerPeerData::handle(cc_string data, size_t size)
{
    MessagePeerData* msgPeerData = (MessagePeerData*)data;

    string peerId;
    peerId.assign(msgPeerData->_id);
    wstring peerNick;
    peerNick.assign(msgPeerData->_nickname, msgPeerData->_nicknameLength);

    map<cc_string, Peer>::iterator it = _chatClient->_peersMap.find(peerId.c_str());

    if (peerId != _chatClient->_thisPeer.GetId() && it == _chatClient->_peersMap.end())
    {
        Logger::GetInstance()->Trace("Found peer: ", peerId, ", adding to peers map");

        wstring peerNick;
        peerNick.assign(msgPeerData->_nickname, msgPeerData->_nicknameLength);
        wcout << peerNick;
        cout << " entered chat." << endl;

        Peer foundPeer(peerNick, peerId);
        foundPeer.SetIp(_chatClient->_recvEndpoint.address().to_string());
        foundPeer.SetLastActivityCheck(time(0));
        _chatClient->_peersMap.insert(pair<cc_string, Peer>(peerId.c_str(), foundPeer));
    }    
}

void ChatClient::HandlerFileInfo::handle(cc_string data, size_t)
{
    MessageFileInfo* mfb = (MessageFileInfo*)data;
    auto_ptr< UploadingFilesContext > ctx(new UploadingFilesContext);

    // maybe we have the same already (is downloading)
    stringstream ss;
    ss << _chatClient->_recvEndpoint.address().to_string() << ":" << mfb->_id;
    string key(ss.str());

    UploadingFilesMap::iterator it = _chatClient->_uploadingFiles.find(key);
    if (it != _chatClient->_uploadingFiles.end())
    {
        ss.str(string());
        ss << "This file is already downloading '" << mfb->_name << "'";
        _chatClient->PrintSystemMsg(_chatClient->_recvEndpoint, ss.str());
        return;
    }

    // try to open

    if (mfb->_nameLength > 256)
        mfb->_nameLength = 256;

    mfb->_name[mfb->_nameLength] = '\0';

    ctx->fp.open(mfb->_name, ios_base::out | ios_base::trunc | ios_base::binary);

    if (!ctx->fp.is_open())
    {
        ss.str(string());
        ss << "can't open for writing '" << mfb->_name << "'";
        _chatClient->PrintSystemMsg(_chatClient->_recvEndpoint, ss.str());
        return;
    }

    Logger::GetInstance()->Trace("Start downloading file ", mfb->_name);

    // fill in the fields
    ctx->endpoint = _chatClient->_recvEndpoint;
    ctx->endpoint.port(_chatClient->_port);
    ctx->id = mfb->_id;
    ctx->blocks = mfb->_totalBlocks;
    ctx->resendCount = 0;
    ctx->blocksReceived = 0;
    ctx->name = string(mfb->_name, mfb->_nameLength);
    ctx->ts = time(0);

    // requesting the first packet
    _chatClient->SendResendMsgInternal(ctx.get());

    // save this for the next use
    _chatClient->_uploadingFiles[key] = ctx.release();
    return;
}

void ChatClient::HandlerFileBlock::handle(cc_string data, size_t)
{
    MessageFileBlock* mfp = (MessageFileBlock*)data;

    stringstream ss;
    ss << _chatClient->_recvEndpoint.address().to_string() << ":" << mfp->_id;

    string key(ss.str());
    UploadingFilesContext* ctx = 0;
    UploadingFilesMap::iterator it = _chatClient->_uploadingFiles.find(key);
    if (it != _chatClient->_uploadingFiles.end())
        ctx = _chatClient->_uploadingFiles[key];

    if (ctx == 0 || mfp->_block >= ctx->blocks)
    {
        Logger::GetInstance()->Trace("Received block for file ", ctx->name, " is out of queue!");
        return;
    }

    ctx->fp.write(mfp->_data, mfp->_size);
    ctx->resendCount = 0;
    ctx->blocksReceived += 1;
    ctx->ts = time(0);

    // we have all blocks?

    if (ctx->blocks == ctx->blocksReceived)
    {
        Logger::GetInstance()->Trace("Done loading file ", ctx->name);
        _chatClient->_uploadingFiles.erase(_chatClient->_uploadingFiles.find(key));
        ctx->fp.close();
        delete ctx;
        return;
    }

    if (!(mfp->_block % 89))
    {
        ss.str(string());
        ss << "Downloaded block " << mfp->_block << " from " << ctx->blocks;
        Logger::GetInstance()->Trace("File: ", ctx->name, ". ", ss.str());
    }

    // requesting the next block

    _chatClient->SendTo(ctx->endpoint, MessageBuilder::RequestForFileBlock(ctx->id, ctx->blocksReceived, string())); //TODO: remove the stub

    return;
}

void ChatClient::HandlerRequestForFileBlock::handle(cc_string data, size_t)
{
    MessageRequestForFileBlock* mrfp = (MessageRequestForFileBlock*)data;

    if (SIMULATE_PACKET_LOOSING == 1)
    {
        srand((uint32)time(NULL));
        if ((rand() % 10 + 1) > 7)
            return;
    }

    if (_chatClient->_sendingFiles.find(mrfp->_id) == _chatClient->_sendingFiles.end())
        return;

    SendingFilesContext * fsc = _chatClient->_sendingFiles[mrfp->_id];
    if (mrfp->_block >= fsc->totalBlocks)
        return;

    // open file

    ifstream input;
    input.open(fsc->path.c_str(), ifstream::in | ios_base::binary);
    if (!input.is_open())
    {
        Logger::GetInstance()->Trace("File: ", fsc->path, ". Can't open it!");
        return;
    }

    // read and send needed block

    char raw[FILE_BLOCK_MAX];
    input.seekg(mrfp->_block * FILE_BLOCK_MAX);
    input.read(raw, sizeof(raw));

    _chatClient->SendTo(fsc->endpoint, MessageBuilder::FileBlock(mrfp->_id, mrfp->_block, raw, (uint32)input.gcount(), string())); //TODO: remove the stub

    input.close();

    if (mrfp->_block == 0)
        fsc->firstBlockSent = true;
}
