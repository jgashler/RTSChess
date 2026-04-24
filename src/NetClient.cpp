#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOGDI
#  define NOMINMAX
#endif

#include "NetClient.h"
#include <rtc/rtc.hpp>
#include <cstdio>
#include <cstring>

NetClient::~NetClient() { Disconnect(); }

bool NetClient::SetOffer(const std::string& offerSdp) {
    rtc::Configuration cfg;
    cfg.iceServers.emplace_back("stun:stun.l.google.com:19302");

    pc = std::make_shared<rtc::PeerConnection>(cfg);

    // When ICE gathering is complete the answer SDP is final.
    pc->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) {
        if (state != rtc::PeerConnection::GatheringState::Complete) return;
        auto desc = pc->localDescription();
        if (!desc) return;
        std::lock_guard<std::mutex> lk(mtx);
        pendingAnswer = std::string(*desc);
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

    // The host creates the data channel; we receive it here.
    pc->onDataChannel([this](std::shared_ptr<rtc::DataChannel> channel) {
        dc = channel;
        dc->onMessage([this](rtc::message_variant data) {
            if (!std::holds_alternative<rtc::binary>(data)) return;
            auto& bin   = std::get<rtc::binary>(data);
            auto* bytes = reinterpret_cast<const uint8_t*>(bin.data());
            size_t len  = bin.size();
            if (len < 1) return;

            if ((PacketType)bytes[0] == PacketType::GAME_STATE &&
                len >= sizeof(GameStatePacket)) {
                GameStatePacket pkt;
                std::memcpy(&pkt, bytes, sizeof(pkt));
                std::lock_guard<std::mutex> lk(mtx);
                pendingStates.push_back(pkt);
            }
        });
    });

    try {
        // Set the host's offer, then generate our answer.
        pc->setRemoteDescription(rtc::Description(offerSdp, "offer"));
        pc->setLocalDescription();   // generates answer + triggers ICE gathering
    } catch (const std::exception& e) {
        printf("[Client] SetOffer error: %s\n", e.what());
        return false;
    }
    return true;
}

void NetClient::Poll() {
    std::string                  answer;
    bool                         didConnect = false;
    std::vector<GameStatePacket> states;
    {
        std::lock_guard<std::mutex> lk(mtx);
        answer           = std::move(pendingAnswer);
        didConnect       = pendingConnected;
        pendingConnected = false;
        states           = std::move(pendingStates);
    }
    if (!answer.empty() && onAnswer)    onAnswer(answer);
    if (didConnect      && onConnected) onConnected();
    for (auto& pkt : states)
        if (onState) onState(pkt);
}

void NetClient::SendMoveRequest(uint8_t pieceId, int destX, int destY) {
    if (!dc || !dc->isOpen()) return;
    MoveRequestPacket pkt;
    pkt.pieceId = pieceId;
    pkt.destX   = (int8_t)destX;
    pkt.destY   = (int8_t)destY;
    try {
        rtc::binary bin(
            reinterpret_cast<const std::byte*>(&pkt),
            reinterpret_cast<const std::byte*>(&pkt) + sizeof(pkt));
        dc->send(std::move(bin));
    } catch (...) {}
}

void NetClient::SendRestartRequest() {
    if (!dc || !dc->isOpen()) return;
    RestartRequestPacket pkt;
    try {
        rtc::binary bin(
            reinterpret_cast<const std::byte*>(&pkt),
            reinterpret_cast<const std::byte*>(&pkt) + sizeof(pkt));
        dc->send(std::move(bin));
    } catch (...) {}
}

bool NetClient::IsConnected() const {
    std::lock_guard<std::mutex> lk(mtx);
    return connected;
}

void NetClient::Disconnect() {
    if (dc) { try { dc->close(); } catch (...) {} dc.reset(); }
    if (pc) { try { pc->close(); } catch (...) {} pc.reset(); }
    std::lock_guard<std::mutex> lk(mtx);
    connected = false;
}
