#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOGDI
#  define NOMINMAX
#endif

#include "NetClient.h"
#include <enet/enet.h>
#include <cstdio>
#include <cstring>

bool NetClient::Connect(const char* address, uint16_t port) {
    host = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!host) {
        printf("[Client] Failed to create ENet host.\n");
        return false;
    }

    ENetAddress addr;
    enet_address_set_host(&addr, address);
    addr.port = port;

    peer = enet_host_connect(host, &addr, 2, 0);
    if (!peer) {
        printf("[Client] Failed to initiate connection.\n");
        return false;
    }

    ENetEvent ev;
    if (enet_host_service(host, &ev, 5000) > 0 &&
        ev.type == ENET_EVENT_TYPE_CONNECT) {
        printf("[Client] Connected to %s:%d\n", address, port);
        return true;
    }

    printf("[Client] Connection to %s:%d timed out.\n", address, port);
    enet_peer_reset(peer);
    peer = nullptr;
    return false;
}

void NetClient::Poll() {
    if (!host) return;

    ENetEvent ev;
    while (enet_host_service(host, &ev, 0) > 0) {
        switch (ev.type) {
            case ENET_EVENT_TYPE_RECEIVE: {
                if (ev.packet->dataLength >= 1) {
                    auto ptype = (PacketType)ev.packet->data[0];
                    if (ptype == PacketType::GAME_STATE &&
                        ev.packet->dataLength >= sizeof(GameStatePacket)) {
                        GameStatePacket pkt;
                        std::memcpy(&pkt, ev.packet->data, sizeof(pkt));
                        if (onState) onState(pkt);
                    }
                }
                enet_packet_destroy(ev.packet);
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT:
                printf("[Client] Disconnected from server.\n");
                peer = nullptr;
                break;

            default: break;
        }
    }
}

bool NetClient::IsConnected() const {
    return peer && peer->state == ENET_PEER_STATE_CONNECTED;
}

void NetClient::SendMoveRequest(uint8_t pieceId, int destX, int destY) {
    if (!peer) return;

    MoveRequestPacket pkt;
    pkt.pieceId = pieceId;
    pkt.destX   = (int8_t)destX;
    pkt.destY   = (int8_t)destY;

    ENetPacket* ep = enet_packet_create(
        &pkt, sizeof(pkt), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, ep);
    enet_host_flush(host);
}

void NetClient::Disconnect() {
    if (peer) { enet_peer_disconnect(peer, 0); peer = nullptr; }
    if (host) { enet_host_destroy(host);        host = nullptr; }
}
