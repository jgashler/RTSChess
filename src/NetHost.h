#pragma once
#include "Packets.h"
#include <functional>
#include <string>
#include <memory>
#include <mutex>
#include <vector>

// Forward-declare libdatachannel types so rtc/rtc.hpp is NOT included here.
// Including rtc/rtc.hpp in a header pulls in <winsock2.h> on Windows, which
// conflicts with raylib.h macros in any TU that includes both.
namespace rtc {
    class PeerConnection;
    class DataChannel;
}

class NetHost {
public:
    using MoveCallback      = std::function<void(uint8_t pieceId, int x, int y)>;
    using OfferCallback     = std::function<void(const std::string& sdp)>;
    using ConnectedCallback = std::function<void()>;

    // Start ICE gathering.  onOffer fires (via Poll) when the full offer is ready.
    bool Init();

    // Feed the remote answer SDP after the user pastes it.
    void SetAnswer(const std::string& answerSdp);

    // Call every frame — dispatches pending callbacks on the main thread.
    void Poll();

    void BroadcastState(const GameStatePacket& pkt);

    void SetMoveCallback     (MoveCallback      cb) { onMove      = std::move(cb); }
    void SetOfferCallback    (OfferCallback     cb) { onOffer     = std::move(cb); }
    void SetConnectedCallback(ConnectedCallback cb) { onConnected = std::move(cb); }

    bool IsConnected() const;
    void Shutdown();

    // Destructor defined in .cpp where rtc types are complete
    ~NetHost();

private:
    // shared_ptr is used so the deleter is captured at make_shared time (in the .cpp
    // where the type is complete), allowing storage here with only a forward declaration.
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::DataChannel>    dc;

    MoveCallback      onMove;
    OfferCallback     onOffer;
    ConnectedCallback onConnected;

    struct MoveEvent { uint8_t id; int x, y; };

    mutable std::mutex     mtx;
    std::string            pendingOffer;
    bool                   pendingConnected = false;
    std::vector<MoveEvent> pendingMoves;
    bool                   connected        = false;
};
