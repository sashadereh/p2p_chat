#include "ChatClient.h"
#include "MessageBuilder.h"
#include "message_formats.h"
#include "Logger.h"

ChatClient* ChatClient::Handler::_chatClient = 0;

void ChatClient::HandlerSys::handle(cc_string data, size_t size)
{
    MessageSys* msgSys = (MessageSys*)data;

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
            _chatClient->_peersMap.erase(it);
        }
    }
    else if (action == "alive")
    {
        if (peerId == _chatClient->_thisPeer.GetId())
        {
            return;
        }
        else if (it == _chatClient->_peersMap.end())
        {
            Logger::GetInstance()->Trace("Received 'alive' message from unknown peer ", peerId);
        }
        else
        {
            // Here we should have a ScopedLock
            it->second.SetAliveCheck(time(0));
        }
    }
}

void ChatClient::HandlerText::handle(cc_string data, size_t size)
{
    MessageText* msgText = (MessageText*)data;

    string peerId;
    peerId.assign(msgText->_peerId);

    for (const auto it : _chatClient->_peersMap)
    {
        Logger::GetInstance()->Trace("Element of peers map: ", it.first, it.second);
    }

    auto it = _chatClient->_peersMap.find(peerId.c_str());

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
        wcout << endl << it->second.GetNickname() << " > ";
    }
    wcout.write(msgText->text, msgText->length);
    wcout << endl;
}

void ChatClient::HandlerPeerData::handle(cc_string data, size_t size)
{
    MessagePeerData* msgPeerData = (MessagePeerData*)data;

    string peerId;
    peerId.assign(msgPeerData->_id);
    wstring peerNick;
    peerNick.assign(msgPeerData->_nickname, msgPeerData->_nicknameLength);

    if (peerId != _chatClient->_thisPeer.GetId())
    {
        Logger::GetInstance()->Trace("Found peer: ", peerId, ", adding to peers map");

        wstring peerNick;
        peerNick.assign(msgPeerData->_nickname, msgPeerData->_nicknameLength);
        wcout << peerNick;
        cout << " entered chat." << endl;

        Peer foundPeer(peerNick, peerId);
        foundPeer.SetIp(_chatClient->_recvEndpoint.address().to_string());
        foundPeer.SetAliveCheck(time(0));
        _chatClient->_peersMap.insert(pair<cc_string, Peer>(peerId.c_str(), foundPeer));

        // Send our peer data to found peer
        _chatClient->SendPeerDataMsg(_chatClient->ParseEpFromString(foundPeer.GetIp()), _chatClient->_thisPeer.GetNickname(), _chatClient->_thisPeer.GetId());
    }    
}

void ChatClient::HandlerFileBegin::handle(cc_string data, size_t)
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
        _chatClient->PrintSystemMsg(_chatClient->_recvEndpoint, ss.str());
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
        _chatClient->PrintSystemMsg(_chatClient->_recvEndpoint, ss.str());
        return;
    }

    ss.str(string());
    ss << "start loading file '" << mfb->name << "'";
    _chatClient->PrintSystemMsg(_chatClient->_recvEndpoint, ss.str());

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
    _chatClient->SendResendMsgInternal(ctx.get());

    // save this for the next use
    _chatClient->_files[key] = ctx.release();
    return;
}

void ChatClient::HandlerFileBlock::handle(cc_string data, size_t)
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
        _chatClient->PrintSystemMsg(_chatClient->_recvEndpoint, "received block is out of queue!");
        return;
    }

    ctx->fp.write(mfp->data, mfp->size);
    ctx->resendCount = 0;
    ctx->blocksReceived += 1;
    ctx->ts = time(0);

    // we have all blocks?

    if (ctx->blocks == ctx->blocksReceived)
    {
        _chatClient->PrintSystemMsg(ctx->endpoint, "done loading file");
        _chatClient->_files.erase(_chatClient->_files.find(key));
        ctx->fp.close();
        delete ctx;
        return;
    }

    if (!(mfp->block % 89))
    {
        ss.str(string());
        ss << "downloaded block " << mfp->block << " from " << ctx->blocks;
        _chatClient->PrintSystemMsg(_chatClient->_recvEndpoint, ss.str());
    }

    // requesting the next block

    _chatClient->SendTo(ctx->endpoint, MessageBuilder::ResendableFileBlock(ctx->id, ctx->blocksReceived));

    return;
}

void ChatClient::HandlerResendFileBlock::handle(cc_string data, size_t)
{
    MessageResendFileBlock* mrfp = (MessageResendFileBlock*)data;

    if (SIMULATE_PACKET_LOOSING == 1)
    {
        srand((uint32)time(NULL));
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
        _chatClient->PrintSystemMsg(_chatClient->_recvEndpoint, "can't open file.");
        return;
    }

    // read and send needed block

    char raw[FILE_BLOCK_MAX];
    input.seekg(mrfp->block * FILE_BLOCK_MAX);
    input.read(raw, sizeof(raw));

    _chatClient->SendTo(fsc->endpoint, MessageBuilder::FileBlock(mrfp->id, mrfp->block, raw, (uint32)input.gcount()));

    input.close();

    if (mrfp->block == 0)
        fsc->firstBlockSent = true;
}
