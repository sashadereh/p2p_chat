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

    Peers::iterator it = _chatClient->_peersMap.find(peerId);

    if (peerId == _chatClient->_thisPeer.GetId())
    {
        return;
    }
    else if (it == _chatClient->_peersMap.end())
    {
        Logger::GetInstance()->Trace("Received '", action, "' message from unknown peer ", peerId);
        return;
    }

    Peer* peer = (*it).second;

    peer->SetLastActivityCheck(time(0));
    peer->SetPingSent(false);

    if (action == "quit")
    {
        wcout << peer->GetNickname();
        cout << " left out chat." << endl;
        Logger::GetInstance()->Trace("Peer ", peer->GetId(), " left out chat. Removing it from peers map");
        _chatClient->_peersMap.erase(it->first);
    }
    else if (action == "ping")
    {
        UdpEndpoint endp = _chatClient->_recvEndpoint;
        endp.port(_chatClient->_port);
        _chatClient->SendTo(endp, MessageBuilder::System("pong", _chatClient->_thisPeer.GetId()));
    }
    else if (action == "filedone")
    {
        Logger::GetInstance()->Trace("File was successfully sent");
        cout << "\n File was successfully sent" << endl;
    }
}

void ChatClient::HandlerText::handle(cc_string data, size_t size)
{
    MessageText* msgText = (MessageText*)data;

    string peerId;
    peerId.assign(msgText->_peerId);

    Peers::iterator it = _chatClient->_peersMap.find(peerId);

    if (peerId == _chatClient->_thisPeer.GetId())
    {
        wcout << _chatClient->_thisPeer.GetNickname() << " > ";
    }
    else if (it == _chatClient->_peersMap.end())
    {
        Logger::GetInstance()->Trace("Received text message from unknown peer ", peerId);
        return;
    }
    else
    {
        Peer* peer = (*it).second;
        peer->SetLastActivityCheck(time(0));
        peer->SetPingSent(false);
        wcout << peer->GetNickname() << " > ";
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

    Peers::iterator it = _chatClient->_peersMap.find(peerId);

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

    Peers::iterator peer_it = _chatClient->_peersMap.find(peerId);

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
        cout << ss.str();
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
        ss << "Can't open for writing '" << msgFileInfo->_name << "'";
        cout << ss.str();
        return;
    }
    
    Logger::GetInstance()->Trace("Start uploading file ", msgFileInfo->_name);
    cout << "\nStart uploading file " << msgFileInfo->_name << endl;

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
    _chatClient->SendReqForFileBlockMsg(ctx.get());

    // save this for the next use
    _chatClient->_uploadingFiles[key] = ctx.release();
    return;
}

void ChatClient::HandlerFileBlock::handle(cc_string data, size_t)
{
    MessageFileBlock* msgFileBlock = (MessageFileBlock*)data;

    string peerId;
    peerId.assign(msgFileBlock->_peerId);

    Peers::iterator peer_it = _chatClient->_peersMap.find(peerId);

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

    ss.str(string());
    ss << "File: " << ctx->name << ". " << "Downloaded block " << ctx->blocksReceived << " from " << ctx->blocks;
    Logger::GetInstance()->Trace(ss.str());
    cout << endl << ss.str() << endl;

    if (ctx->blocks == ctx->blocksReceived)
    {
        Logger::GetInstance()->Trace("Done uploading file ", ctx->name);
        cout << "\nDone uploading file " << ctx->name << endl;
        _chatClient->_uploadingFiles.erase(_chatClient->_uploadingFiles.find(key));
        ctx->fp.close();
        _chatClient->SendTo(ctx->endpoint, MessageBuilder::System("filedone", _chatClient->_thisPeer.GetId()));
        delete ctx;
        return;
    }

    // requesting the next block

    _chatClient->SendTo(ctx->endpoint, MessageBuilder::RequestForFileBlock(ctx->id, ctx->blocksReceived, _chatClient->_thisPeer.GetId()));

    return;
}

void ChatClient::HandlerRequestForFileBlock::handle(cc_string data, size_t)
{
    MessageRequestForFileBlock* msgReqForFileBlock = (MessageRequestForFileBlock*)data;

    string peerId;
    peerId.assign(msgReqForFileBlock->_peerId);

    Peers::iterator peer_it = _chatClient->_peersMap.find(peerId);

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

    _chatClient->SendTo(fsc->endpoint,
        MessageBuilder::FileBlock(msgReqForFileBlock->_id, msgReqForFileBlock->_block, raw,
        (uint32)input.gcount(), _chatClient->_thisPeer.GetId()));

    input.close();

    if (msgReqForFileBlock->_block == 0)
        fsc->firstBlockSent = true;
}
