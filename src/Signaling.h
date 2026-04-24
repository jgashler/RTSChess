#pragma once
#include <string>
#include <functional>

// Async HTTP signaling client for the Cloudflare Worker.
// All Async* calls spawn a background thread; call Poll() every frame
// to dispatch result callbacks safely on the main thread.
namespace Signaling {

    // POST /host {offer SDP} → room code string, or "" on error
    void AsyncPostOffer(const std::string& offer,
                        std::function<void(std::string code)> cb);

    // GET /join/{code} → offer SDP, or "" if room not found
    void AsyncGetOffer(const std::string& code,
                       std::function<void(std::string offer)> cb);

    // POST /answer/{code} {answer SDP} → true on success
    void AsyncPostAnswer(const std::string& code,
                         const std::string& answer,
                         std::function<void(bool ok)> cb);

    // GET /answer/{code} → answer SDP once ready, or "" if not yet
    void AsyncPollAnswer(const std::string& code,
                         std::function<void(std::string answer)> cb);

    // Dispatch any pending callbacks on the main thread. Call every frame.
    void Poll();

} // namespace Signaling
