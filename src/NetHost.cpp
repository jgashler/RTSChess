#include "NetHost.h"
#include "Game.h"
#include "Board.h"
#include "Piece.h"
#include <cstdio>
#include <cstring>

bool NetHost::Init(uint16_t port) {
    ENetAddress addr;
    addr.host = ENET_HOST_ANY;
    addr.port = port;

    host = enet_host_create(&addr,
                            1,      // max clients
                            2,      // channels
                            0, 0);  // unlimited bandwidth
    if (!host) {
        printf("[Host] Failed to create ENet host on port %d\n", port);
        return false;
    }
    printf("[Host] Listening on port %d...\n", port);
    return true;
}

void NetHost::Poll(Game& game) {
    if (!host) return;

    ENetEvent ev;
    while (enet_host_service(host, &ev, 0) > 0) {
        switch (ev.type) {
            case ENET_EVENT_TYPE_CONNECT:
                peer = ev.peer;
                printf("[Host] Client connected.\n");
                break;

            case ENET_EVENT_TYPE_RECEIVE: {
                if (ev.packet->dataLength < 1) break;
                auto ptype = (PacketType)ev.packet->data[0];

                if (ptype == PacketType::MOVE_REQUEST &&
                    ev.packet->dataLength >= sizeof(MoveRequestPacket)) {
                    MoveRequestPacket req;
                    std::memcpy(&req, ev.packet->data, sizeof(req));
                    game.ExecuteMoveRequest(req.pieceId, {req.destX, req.destY});
                }
                enet_packet_destroy(ev.packet);
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT:
                printf("[Host] Client disconnected.\n");
                peer = nullptr;
                break;

            default: break;
        }
    }
}

void NetHost::BroadcastState(const GameStatePacket& pkt) {
    if (!host || !peer) return;

    ENetPacket* ep = enet_packet_create(
        &pkt, sizeof(pkt),
        ENET_PACKET_FLAG_UNSEQUENCED   // latest-state: drop old, keep newest
    );
    enet_peer_send(peer, 1, ep);       // channel 1 = unreliable state
    enet_host_flush(host);
}

void NetHost::Shutdown() {
    if (peer)  { enet_peer_disconnect(peer, 0); peer = nullptr; }
    if (host)  { enet_host_destroy(host);        host = nullptr; }
}
