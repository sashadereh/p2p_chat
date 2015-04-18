You can find here peer-to-peer chat client for the local network.
It uses boost library and only UDP as a transport protocol.

#You can:

1. Talk to peers, who run this chat client in the local network.

2. Talk to only one peer by sending private messages.

3. Send files to the peer. Packets with blocks of the file, that are lost due to UDP are re-downloaded.

4. TODO

#Usage:

Sending message to all peers: just write it and press "Enter"

Sending private message: @ [ip] [message]

Sending file: file [ip] [path]
