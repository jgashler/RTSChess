#pragma once
#include "Packets.h"
#include <functional>
#include <string>
#include <memory>
#include <mutex>
#include <vector>

namespace rtc {
    class PeerConnection;
    class DataChannel;
}

class NetClient {
public:
    using StateCallback     = std::function<void(const GameStatePacket&)>;
    using AnswerCallback    = std::function<void(const std::string& sdp)>;
    using ConnectedCallback = std::function<void()>;

    // Feed the host's offer SDP; triggers answer generation + ICE gathering.
    // onAnswer fires (via Poll) when the full answer SDP is ready to copy.
    bool SetOffer(const std::string& offerSdp);

    // Call every frame — dispatches pending callbacks on the main thread.
    void Poll();

    void SendMoveRequest(uint8_t pieceId, int destX, int destY);
    void SendRestartRequest();

    void SetStateCallback    (StateCallback     cb) { onState     = std::move(cb); }
    void SetAnswerCallback   (AnswerCallback    cb) { onAnswer    = std::move(cb); }
    void SetConnectedCallback(ConnectedCallback cb) { onConnected = std::move(cb); }

    bool IsConnected() const;
    void Disconnect();

    ~NetClient();

private:
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::DataChannel>    dc;

    StateCallback     onState;
    AnswerCallback    onAnswer;
    ConnectedCallback onConnected;

    mutable std::mutex           mtx;
    std::string                  pendingAnswer;
    bool                         pendingConnected = false;
    std::vector<GameStatePacket> pendingStates;
    bool                         connected        = false;
};
