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

    PeersMap::iterator it = _chatClient->_peersMap.find(peerId);

    if (peerId == _chatClient->_thisPeer.GetId())
    {
        return;
    }
    else if (it == _chatClient->_peersMap.end())
    {
        Logger::GetInstance()->Trace("Received '", action, "' message from unknown peer ", peerId);
        Logger::GetInstance()->Trace("Size of the map: \n", _chatClient->_peersMap.size());
        for (const auto& ite : _chatClient->_peersMap)
        {
            Logger::GetInstance()->Trace("Element: first = ", ite.first, " second.Id = ", ite.second->GetId());
        }
        return;
    }

    Peer* peer = (*it).second;

    peer->SetLastActivityCheck(time(0));
    peer->SetPingSent(false);

    if (action == "quit")
    {
        wcout << peer->GetNickname();
        cout << " left out chat." << endl;
        Logger::GetInstance()->Trace("Peer ", peer->GetId(), " is offline. Removing from peers map");
        _chatClient->_peersMap.erase(it->first);
    }
    else if (action == "ping")
    {
        UdpEndpoint endp = _chatClient->_recvEndpoint;
        endp.port(_chatClient->_port);
        _chatClient->SendTo(endp, MessageBuilder::System("pong", _chatClient->_thisPeer.GetId()));
    }
}

void ChatClient::HandlerText::handle(cc_string data, size_t size)
{
    MessageText* msgText = (MessageText*)data;

    string peerId;
    peerId.assign(msgText->_peerId);

    PeersMap::iterator it = _chatClient->_peersMap.find(peerId);

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
        Peer* peer = (*it).second;
        Logger::GetInstance()->Trace("Received text message from ", peerId);
        peer->SetLastActivityCheck(time(0));
        peer->SetPingSent(false);
        wcout << endl << peer->GetNickname() << " > ";
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

    PeersMap::iterator it = _chatClient->_peersMap.find(peerId);

    if (peerId != _chatClient->_thisPeer.GetId() && it == _chatClient->_peersMap.end())
    {
        Logger::GetInstance()->Trace("Found peer: ", peerId, ", adding to peers map");

        wcout << peerNick;
        cout << " entered chat." << endl;

        Peer* peer = new Peer(peerNick, peerId);
        peer->SetIp(_chatClient->_recvEndpoint.address().to_string());
        peer->SetLastActivityCheck(time(0));
        peer->SetPingSent(false);
        _chatClient->_peersMap[peerId] = peer;
        UdpEndpoint endp = _chatClient->_recvEndpoint;
        endp.port(_chatClient->_port);
        _chatClient->SendTo(endp, MessageBuilder::PeerData(_chatClient->_thisPeer.GetNickname(), _chatClient->_thisPeer.GetId()));
    }    
}

void ChatClient::HandlerFileInfo::handle(cc_string data, size_t)
{
    MessageFileInfo* msgFileInfo = (MessageFileInfo*)data;

    string peerId;
    peerId.assign(msgFileInfo->_peerId);

    PeersMap::iterator peer_it = _chatClient->_peersMap.find(peerId);

    if (peerId == _chatClient->_thisPeer.GetId())
    {
        return;
    }
    else if (peer_it == _chatClient->_peersMap.end())
    {
        Logger::GetInstance()->Trace("Received file info message from unknown peer ", peerId);
        return;
    }
    Peer* peer = (*peer_it).second;

    peer->SetLastActivityCheck(time(0));
    peer->SetPingSent(false);

    auto_ptr< UploadingFilesContext > ctx(new UploadingFilesContext);

    // maybe we have the same already (is downloading)
    stringstream ss;
    ss << _chatClient->_recvEndpoint.address().to_string() << ":" << msgFileInfo->_id;
    string key(ss.str());

    UploadingFilesMap::iterator it = _chatClient->_uploadingFiles.find(key);
    if (it != _chatClient->_uploadingFiles.end())
    {
        ss.str(string());
        ss << "This file is already downloading '" << msgFileInfo->_name << "'";
        _chatClient->PrintSystemMsg(_chatClient->_recvEndpoint, ss.str());
        return;
    }

    // try to open

    if (msgFileInfo->_nameLength > 256)
        msgFileInfo->_nameLength = 256;

    msgFileInfo->_name[msgFileInfo->_nameLength] = '\0';

    ctx->fp.open(msgFileInfo->_name, ios_base::out | ios_base::trunc | ios_base::binary);

    if (!ctx->fp.is_open())
    {
        ss.str(string());
        ss << "can't open for writing '" << msgFileInfo->_name << "'";
        _chatClient->PrintSystemMsg(_chatClient->_recvEndpoint, ss.str());
        return;
    }

    Logger::GetInstance()->Trace("Start downloading file ", msgFileInfo->_name);

    // fill in the fields
    ctx->endpoint = _chatClient->_recvEndpoint;
    ctx->endpoint.port(_chatClient->_port);
    ctx->id = msgFileInfo->_id;
    ctx->blocks = msgFileInfo->_totalBlocks;
    ctx->resendCount = 0;
    ctx->blocksReceived = 0;
    ctx->name = string(msgFileInfo->_name, msgFileInfo->_nameLength);
    ctx->ts = time(0);

    // requesting the first packet
    _chatClient->SendResendMsgInternal(ctx.get());

    // save this for the next use
    _chatClient->_uploadingFiles[key] = ctx.release();
    return;
}

void ChatClient::HandlerFileBlock::handle(cc_string data, size_t)
{
    MessageFileBlock* msgFileBlock = (MessageFileBlock*)data;

    string peerId;
    peerId.assign(msgFileBlock->_peerId);

    PeersMap::iterator peer_it = _chatClient->_peersMap.find(peerId);

    if (peerId == _chatClient->_thisPeer.GetId())
    {
        return;
    }
    else if (peer_it == _chatClient->_peersMap.end())
    {
        Logger::GetInstance()->Trace("Received file block from unknown peer ", peerId);
        return;
    }
    Peer* peer = (*peer_it).second;

    peer->SetLastActivityCheck(time(0));
    peer->SetPingSent(false);

    stringstream ss;
    ss << _chatClient->_recvEndpoint.address().to_string() << ":" << msgFileBlock->_id;

    string key(ss.str());
    UploadingFilesContext* ctx = 0;
    UploadingFilesMap::iterator it = _chatClient->_uploadingFiles.find(key);
    if (it != _chatClient->_uploadingFiles.end())
        ctx = _chatClient->_uploadingFiles[key];

    if (ctx == 0 || msgFileBlock->_block >= ctx->blocks)
    {
        Logger::GetInstance()->Trace("Received block for file ", ctx->name, " is out of queue!");
        return;
    }

    ctx->fp.write(msgFileBlock->_data, msgFileBlock->_size);
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

    if (!(msgFileBlock->_block % 89))
    {
        ss.str(string());
        ss << "Downloaded block " << msgFileBlock->_block << " from " << ctx->blocks;
        Logger::GetInstance()->Trace("File: ", ctx->name, ". ", ss.str());
    }

    // requesting the next block

    _chatClient->SendTo(ctx->endpoint, MessageBuilder::RequestForFileBlock(ctx->id, ctx->blocksReceived, string())); //TODO: remove the stub

    return;
}

void ChatClient::HandlerRequestForFileBlock::handle(cc_string data, size_t)
{
    MessageRequestForFileBlock* msgReqForFileBlock = (MessageRequestForFileBlock*)data;

    string peerId;
    peerId.assign(msgReqForFileBlock->_peerId);

    PeersMap::iterator peer_it = _chatClient->_peersMap.find(peerId);

    if (peerId == _chatClient->_thisPeer.GetId())
    {
        return;
    }
    else if (peer_it == _chatClient->_peersMap.end())
    {
        Logger::GetInstance()->Trace("Received request for file block from unknown peer ", peerId);
        return;
    }
    Peer* peer = (*peer_it).second;

    peer->SetLastActivityCheck(time(0));
    peer->SetPingSent(false);

    if (SIMULATE_PACKET_LOOSING == 1)
    {
        srand((uint32)time(NULL));
        if ((rand() % 10 + 1) > 7)
            return;
    }

    if (_chatClient->_sendingFiles.find(msgReqForFileBlock->_id) == _chatClient->_sendingFiles.end())
        return;

    SendingFilesContext * fsc = _chatClient->_sendingFiles[msgReqForFileBlock->_id];
    if (msgReqForFileBlock->_block >= fsc->totalBlocks)
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
    input.seekg(msgReqForFileBlock->_block * FILE_BLOCK_MAX);
    input.read(raw, sizeof(raw));

    _chatClient->SendTo(fsc->endpoint, MessageBuilder::FileBlock(msgReqForFileBlock->_id, msgReqForFileBlock->_block, raw, (uint32)input.gcount(), string())); //TODO: remove the stub

    input.close();

    if (msgReqForFileBlock->_block == 0)
        fsc->firstBlockSent = true;
}
