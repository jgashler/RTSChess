#pragma once
#include <enet/enet.h>
#include "Packets.h"

class Game;

class NetClient {
public:
    // Returns true when connection handshake completes.
    bool Connect(const char* address, uint16_t port = 7777);

    // Call every frame: dispatches incoming state packets.
    void Poll(Game& game);

    // Send a move request to the server.
    void SendMoveRequest(uint8_t pieceId, int destX, int destY);

    bool IsConnected() const {
        return peer && peer->state == ENET_PEER_STATE_CONNECTED;
    }

    void Disconnect();
    ~NetClient() { Disconnect(); }

private:
    ENetHost* host = nullptr;
    ENetPeer* peer = nullptr;
};
