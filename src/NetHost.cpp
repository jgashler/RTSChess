// rtc/rtc.hpp pulls in WinSock on Windows.  Define lean-headers first so it
// doesn't conflict with anything — this file never includes raylib.h / Game.h.
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOGDI
#  define NOMINMAX
#endif

#include "NetHost.h"
#include <rtc/rtc.hpp>
#include <cstdio>
#include <cstring>

NetHost::~NetHost() { Shutdown(); }

bool NetHost::Init() {
    rtc::Configuration cfg;
    // Google's free STUN server — only used to discover the public IP/port;
    // actual game packets travel directly peer-to-peer.
    cfg.iceServers.emplace_back("stun:stun.l.google.com:19302");

    pc = std::make_shared<rtc::PeerConnection>(cfg);

    // When all ICE candidates are gathered the local description is final.
    // Store it then; that's what we show the user to copy & send.
    pc->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) {
        if (state != rtc::PeerConnection::GatheringState::Complete) return;
        auto desc = pc->localDescription();
        if (!desc) return;
        std::lock_guard<std::mutex> lk(mtx);
        pendingOffer = std::string(*desc);
    });

    pc->onStateChange([this](rtc::PeerConnection::State state) {
        if (state == rtc::PeerConnection::State::Connected) {
            std::lock_guard<std::mutex> lk(mtx);
            pendingConnected = true;
            connected        = true;
        } else if (state == rtc::PeerConnection::State::Failed ||
                   state == rtc::PeerConnection::State::Closed) {
            std::lock_guard<std::mutex> lk(mtx);
            connected = false;
        }
    });

    // Host creates the data channel (the remote side receives it via onDataChannel).
    dc = pc->createDataChannel("game");

    dc->onMessage([this](rtc::message_variant data) {
        if (!std::holds_alternative<rtc::binary>(data)) return;
        auto& bin   = std::get<rtc::binary>(data);
        auto* bytes = reinterpret_cast<const uint8_t*>(bin.data());
        size_t len  = bin.size();
        if (len < 1) return;

        if ((PacketType)bytes[0] == PacketType::MOVE_REQUEST &&
            len >= sizeof(MoveRequestPacket)) {
            MoveRequestPacket req;
            std::memcpy(&req, bytes, sizeof(req));
            std::lock_guard<std::mutex> lk(mtx);
            pendingMoves.push_back({ req.pieceId, req.destX, req.destY });
        } else if ((PacketType)bytes[0] == PacketType::RESTART_REQ) {
            std::lock_guard<std::mutex> lk(mtx);
            pendingRestart = true;
        }
    });

    // Trigger offer generation + ICE gathering.
    pc->setLocalDescription();
    return true;
}

void NetHost::SetAnswer(const std::string& answerSdp) {
    if (!pc) return;
    try {
        pc->setRemoteDescription(rtc::Description(answerSdp, "answer"));
    } catch (const std::exception& e) {
        printf("[Host] SetAnswer error: %s\n", e.what());
    }
}

void NetHost::Poll() {
    std::string            offer;
    bool                   didConnect  = false;
    bool                   didRestart  = false;
    std::vector<MoveEvent> moves;
    {
        std::lock_guard<std::mutex> lk(mtx);
        offer            = std::move(pendingOffer);
        didConnect       = pendingConnected;
        pendingConnected = false;
        didRestart       = pendingRestart;
        pendingRestart   = false;
        moves            = std::move(pendingMoves);
    }
    if (!offer.empty()  && onOffer)     onOffer(offer);
    if (didConnect      && onConnected) onConnected();
    if (didRestart      && onRestart)   onRestart();
    for (auto& m : moves)
        if (onMove) onMove(m.id, m.x, m.y);
}

void NetHost::BroadcastState(const GameStatePacket& pkt) {
    if (!dc || !dc->isOpen()) return;
    try {
        rtc::binary bin(
            reinterpret_cast<const std::byte*>(&pkt),
            reinterpret_cast<const std::byte*>(&pkt) + sizeof(pkt));
        dc->send(std::move(bin));
    } catch (...) {}
}

bool NetHost::IsConnected() const {
    std::lock_guard<std::mutex> lk(mtx);
    return connected;
}

void NetHost::Shutdown() {
    if (dc) { try { dc->close(); } catch (...) {} dc.reset(); }
    if (pc) { try { pc->close(); } catch (...) {} pc.reset(); }
    std::lock_guard<std::mutex> lk(mtx);
    connected = false;
}
